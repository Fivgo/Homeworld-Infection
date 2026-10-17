/*=============================================================================
    Name    : mesh.c
    Purpose : Functions to load, manipulate and render polygon meshes.

    Created 6/21/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifdef _WIM32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "glinc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if !defined _MSC_VER
#include <strings.h>
#endif

#include <ctype.h>
#include "Debug.h"
#include "File.h"
#include "Memory.h"
#include "Vector.h"
#include "texreg.h"
#include "render.h"
#include "utility.h"
#include "Teams.h"
#include "ObjTypes.h"
#include "SpaceObj.h"
#include "Universe.h"
#include "Mesh.h"
#include "Shader.h"
#include "AutoLOD.h"
#include "CRC32.h"
#include "Hash.h"
#include "prim3d.h"
#include "MeshAnim.h"       // CAT: /catMeshLoadTest also loads the .mad files (port diagnostic)
#include "Particle.h"       // CAT (M2-R5 render): the edge lightning draws electric bolts
#include "LOD.h"
#include "FastMath.h"
#include "Collision.h"   // CAT (M2-R5 render): cat_collLineTriangleIntersect
#include "ShipDefs.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

#define GLVERTEX(x) glVertex4fv((GLfloat*)(x))
#define VERTEXLIST clipVertexList

#define meshColour(NORMAL) shColourSet0((vector*)NORMAL, NULL, NULL);
//CAT: per-vertex colour (meshObjectRender 0x004b0170, meshObjectRenderLit 0x004b0970): with enableCPV set, each vertex
//is lit with its own info entry (pVertexInfo[iV], cat_shColourAddGlow 0x0050d7c0) and its runtime glow state. The
//exe passes the object's vertex states (0x28 bytes per vertex, from the ship's per-vertex mesh state,
//cat_univShipMeshStateCreate 0x00541cd0) as the new first argument of both functions; here the hierarchy walkers
//(meshObjectRenderHierarchy 0x004b4370, meshObjectRenderR 0x004b4110) set meshCurrentVertexState around the call
//instead, so the HW1 signatures and their other callers stay as they are (NULL: no state).
static struct CatVertexState *meshCurrentVertexState = NULL;
#define MESH_VERTEX_STATE(IV)   ((meshCurrentVertexState != NULL) ? (real32 *)&meshCurrentVertexState[(IV)] : (real32 *)NULL)
//CAT: the vertex states of slot `index` of a level's mesh state, or NULL (the exe's `slots ? slots[index].vertex : 0`)
#define MESH_SLOT_STATE(SLOTS, INDEX)   (((SLOTS) != NULL) ? (SLOTS)[(INDEX)].vertex : NULL)
#define meshColourCPV(NORMAL, OBJECT, IV) \
    shColourSet0((vector*)(NORMAL), MESH_VERTEX_STATE(IV), &(OBJECT)->pVertexInfo[(IV)].colourOffset);
//CAT: the smooth modes of meshObjectRenderLit 0x004b0970 light every vertex: with its info entry and state when
//enableCPV is set, with neither when it is not
#define meshColourLit(NORMAL, OBJECT, IV) \
    if (enableCPV) { meshColourCPV(NORMAL, OBJECT, IV) } else { meshColour(NORMAL) }

//CAT: the specular renderer lights each vertex with its info entry and state when enableCPV is set, with neither when
//it is not (meshSpecObjectRender 0x004b1d40, meshSpecColour inlined: shSpecularColour's last two arguments)
#define meshSpecColourCPV(NORMAL, VERTEXLIST, OBJECT, IV, M, MINV) \
    meshSpecColour((NORMAL), &(VERTEXLIST)[(IV)], (M), (MINV), enableCPV ? MESH_VERTEX_STATE(IV) : NULL, \
                   enableCPV ? &(OBJECT)->pVertexInfo[(IV)].colourOffset : NULL)

#define MESH_OBJECT_RENDER(O, M, C)\
    if (g_SpecHack)\
    {\
        meshSpecObjectRender(O, M, C);\
    }\
    else if (usingShader && rndLightingEnabled && !g_WireframeHack)\
    {\
        meshObjectRenderLit(O, M, C);\
    }\
    else\
    {\
        meshObjectRender(O, M, C);\
    }

/*=============================================================================
    Data:
=============================================================================*/

sdword scaledWidth, scaledHeight;
sdword visiblePoly = -1;
sdword visibleDirection = 0;
sdword visibleWhich = 0;
sdword visibleSegment = 0;
real32 visibleUV[3][2];
bool32 g_SpecificPoly = FALSE;

void meshSpecObjectRender(polygonobject *object, materialentry *materials, sdword iColorScheme);
//CAT (M2-R5 render): the exe's material helpers (below meshCurrentMaterialDefault)
static void cat_meshMaterialTextureAndShade(materialentry *material, sdword iColorScheme);
static void cat_meshEMPFlickerAmbient(GLfloat *attribs);
static void cat_meshMaterialSwitch(materialentry *material, sdword iColorScheme);
//CAT: the exe tests RGLtype (0x00b3a700); this tree draws with OpenGL (GLtype 3, as render.c)
#define CAT_MESH_RGLTYPE    3

bool32 gSelfIllum;

bool32 usingShader = TRUE;

bool32 bFade = FALSE;
real32 meshFadeAlpha = 1.0f;

bool8 g_NoMatSwitch = FALSE;
bool8 g_ReplaceHack = FALSE;
bool8 g_WireframeHack = FALSE;
bool8 g_SpecHack = FALSE;
bool8 g_HiddenRemoval = TRUE;
bool32  g_Points = FALSE;
bool32  g_Output = FALSE;

static sdword specIndex;
static ubyte specColour[4];

#if MESH_LOAD_DUMMY_TEXTURE
texhandle meshTestHandle = TEX_Invalid;
#endif

//current mesh poly mode
sdword meshPolyMode = MPM_Flat;

#if MESH_MATERIAL_STATS
sdword meshTotalMaterials = 0;
sdword meshMaterialChanges = 0;
char meshMaterialStatsString[100] = "";
#endif //MESH_MATERIAL_STATS

#if MESH_HACK_TEAM_COLORS
trcolorinfo meshRace1Colors = {colRGB(129, 152, 97), colRGB(139, 152, 30)};
trcolorinfo meshRace2Colors = {colRGB(17, 17, 145), colRGB(209, 28, 139)};
#endif

//funtion-pointer for changing the current material
void (*meshCurrentMaterial)(materialentry *material, sdword iColorScheme) = meshCurrentMaterialDefault;

#if MESH_SURFACE_NAME_DEBUG
bool32 gTestTexture = FALSE;
#endif

#if MESH_MORPH_DEBUG
bool32 meshMorphDebug = FALSE;
struct
{
    color from, to;
}
meshMorphLineColors[] =
{
    {colRGB(255, 0, 0),     colRGB(128, 0, 0)},
    {colRGB(0, 255, 0),     colRGB(0, 128, 0)},
    {colRGB(0, 0, 255),     colRGB(0, 0, 128)},
    {colRGB(255, 255, 0),   colRGB(128, 128, 0)},
    {colRGB(0, 255, 255),   colRGB(0, 128, 128)},
    {colRGB(255, 0, 255),   colRGB(128, 0, 128)},
    {colBlack, colBlack}
};
#endif

/*=============================================================================
    Functions:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : meshStartup
    Description : startup the mesh module
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void meshStartup()
{
}

/*-----------------------------------------------------------------------------
    Name        : meshShutdown
    Description : shutdown the mesh module
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void meshShutdown()
{
    //nothing here
}

/*-----------------------------------------------------------------------------
    Name        : meshSetFade (Cataclysm 0x004ae700)
    Description : CAT: the fade has 8 channels (0x00a4c3d8..0x00a4c3f4), each holding 1 - fade; the mesh alpha
                  (meshFadeAlpha, 0x00885d30) is their product and bFade (0x00a4c450) is set when it is not 1.0.
                  Channel users (cat_rndRenderObjectList 0x005776f0, rndFade 0x005753d0): 0 distance fade,
                  2 crystal, 3 Beast-stun overlay, 4 mimic warp, 5 cloak fade, 6 salvage beam, 7 docking.
                  HW1 had one fade value.
    Inputs      : channel - 0..7; fade - 0 = opaque, 1 = gone
    Outputs     : meshFadeAlpha, bFade
    Return      :
----------------------------------------------------------------------------*/
//CAT: the exe stores the channel only with the shader (usingShader 0x00885d2c)
static real32 catMeshFadeChannel[MESH_FADE_CHANNELS] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
void meshSetFade(sdword channel, real32 fade)
{
    sdword i;

    dbgAssertOrIgnore(channel >= 0 && channel < MESH_FADE_CHANNELS);
    if (usingShader)
    {
        catMeshFadeChannel[channel] = 1.0f - fade;
    }
    meshFadeAlpha = catMeshFadeChannel[0];
    for (i = 1; i < MESH_FADE_CHANNELS; i++)
    {
        meshFadeAlpha *= catMeshFadeChannel[i];
    }
    bFade = (meshFadeAlpha != 1.0f);
}

/*-----------------------------------------------------------------------------
    Name        : meshSetSpecular
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void meshSetSpecular(sdword mode, ubyte red, ubyte green, ubyte blue, ubyte alpha)
{
    specIndex = mode;
    if (mode == -1)
    {
        g_SpecHack = 0;
    }
    else
    {
        specColour[0] = red;
        specColour[1] = green;
        specColour[2] = blue;
        specColour[3] = alpha;
        g_SpecHack = 1;
    }
}

/*-----------------------------------------------------------------------------
    Name        : meshNameToRace
    Description : Convert a mesh file name to a race
    Inputs      : fileName - name of mesh file
    Outputs     :
    Return      : pointer to either a ship or a missile static info for that
                    race and type or NULL if unknown race
----------------------------------------------------------------------------*/
StaticInfo *meshNameToStaticInfo(char *fileName)
{
    char raceString[16];
    char typeString[64];
    sdword length;
    StaticInfo *info;
    ShipRace race;
    ShipType type;

#ifdef _WIN32
    dbgAssertOrIgnore(strchr(fileName, '\\'));
    length = strchr(fileName, '\\') - fileName;
#else
    dbgAssertOrIgnore(strpbrk(fileName, "\\/"));
    length = strpbrk(fileName, "\\/") - fileName;
#endif
    memcpy(raceString, fileName, length);
    raceString[length] = 0;                                 //make a race name
    fileName += length + 1;
//    dbgAssertOrIgnore(strchr(fileName, '\\'));                      //make a type name
#ifdef _WIN32
    if (strchr(fileName, '\\') == NULL)
#else
    if (strpbrk(fileName, "\\/") == NULL)
#endif
    {
        return(NULL);
    }
#ifdef _WIN32
    length = strchr(fileName, '\\') - fileName;
#else
    length = strpbrk(fileName, "\\/") - fileName;
#endif
    memcpy(typeString, fileName, length);
    typeString[length] = 0;
    if (strcasecmp(raceString, "Derelicts") == 0)
    {                                                       //if it's a derelict
        type = StrToDerelictType(typeString);
        dbgAssertOrIgnore(type != 0xffffffff);
        return((StaticInfo *)&derelictStaticInfos[type]);
    }
    //else it's a race-specific texture, process normally
    race = StrToShipRace(raceString);                       //get the race
    if (race == 0xffffffff)
    {
        race = NUM_RACES;
        return(NULL);
    }
    type = StrToShipType(typeString);
#ifdef HW_PTR_64
    if (type == 0xffffffffffffffff)
#else
    if (type == 0xffffffff)
#endif
    {
        if (strcasecmp(typeString, "MISSILE") == 0)
        {
            info = (StaticInfo *)&missileStaticInfos[race];
        }
        else if (strcasecmp(typeString, "MINE") == 0)
        {
            info = (StaticInfo *)&mineStaticInfos[race];
        }
        else
        {
            dbgWarningf(DBG_Loc, "Don't know how to team colorize '%s' for '%s'", typeString, raceString);
            return(NULL);
        }
    }
    else
    {
        info = (StaticInfo *)GetShipStaticInfo(type, race);
    }
    return(info);
}

/*-----------------------------------------------------------------------------
    Name        : meshTextureRegisterAllPlayers
    Description : Register the named texture for each player who is of a certain race.
    Inputs      : fullName - name of texture.
                  race - race of players to compare to.  If NUM_RACES, there is no
                    race and only one handle is allocated (cookie wast can you say?)
                  meshReference - mesh reference to pass to the texture registry
    Outputs     : allocates a list of texture handles MAX_MULTIPLAYER_PLAYERS in length.
    Return      : pointer to this new list of handles
----------------------------------------------------------------------------*/
trhandle * meshTextureRegisterAllPlayers(char *fullName, void *meshReference)
{
    trhandle handleList[MAX_MULTIPLAYER_PLAYERS], *allocedList;
    sdword index, nRegistered = 0, nHandles = MAX_MULTIPLAYER_PLAYERS;
    StaticInfo *info;

    //allocate and clear handle list
    trhandle *d = ( trhandle *) handleList;
    while (nHandles--) *d++ = TR_Invalid;

    info = meshNameToStaticInfo(fullName);
    if (info == NULL)
    {
        handleList[0] = trTextureRegister(fullName, NULL, meshReference, TRUE);  // CAT: 4th arg 1 (meshTextureRegisterAllPlayers 0x004ae910)
        nRegistered = 1;
    }
    else
    {
        for (index = 0; index < MAX_MULTIPLAYER_PLAYERS; index++)
        {
            dbgAssertOrIgnore(index < MAX_MULTIPLAYER_PLAYERS);
            handleList[index] = TR_Invalid;

            if (utyShipsAlwaysUseOwnerColors || ((ShipStaticInfo *)info)->shipOrDerelict.teamColor[index])
            {                                               //if this player can build this ship
                handleList[index] = trTextureRegister(fullName, //register in this players's colors
                        &teColorSchemes[index].textureColor, meshReference, TRUE);  // CAT: 4th arg 1 (0x004ae910)
                nRegistered++;
            }
        }
    }

    if (nRegistered > 0)
    {
        // copy handleList because it will go out of scope once we leave this function
        allocedList = memAlloc(MAX_MULTIPLAYER_PLAYERS * sizeof(trhandle), "trhandle*MAX_MULTIPLAYER_PLAYERS", NonVolatile);
        memcpy(allocedList, handleList, MAX_MULTIPLAYER_PLAYERS * sizeof(trhandle));
        return allocedList;
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
    Name        : meshTextureNameToPath
    Description : Create a path for a texture from a texture name as it comes
                    from the .geo file.
    Inputs      : mesh - name of mesh file
                  tex - name of texture
    Outputs     : out - path to texture
    Return      : void
----------------------------------------------------------------------------*/
void meshTextureNameToPath(char *out, char *mesh, char *tex)
{
    char *string;
    unsigned int i;
    //make path of mesh path and \\texture\\<shipNameNoExtension\\<textureFileName>
    strcpy(out, mesh);

    string = out;
#ifdef _WIN32
    while (strchr(string, '\\'))
    {                                               //find last backslash
        string = strchr(string, '\\') + 1;
    }
#else
    while (strpbrk(string, "\\/"))
    {                                               /* find last backslash */
        string = strpbrk(string, "\\/") + 1;
    }
#endif
    dbgAssertOrIgnore(string != out);
    dbgAssertOrIgnore(*string);
    *string = 0;                                    //NULL terminate and append texture name

    strcat(out, tex);
    for (i = 0; (out[i] = toupper(out[i])); i++) { }
}

bool32 meshStringContainsPeriod(char* s)
{
    sdword i;

    for (i = 0; i < strlen(s); i++)
    {
        if (s[i] == '.')
        {
            return TRUE;
        }
    }

    return FALSE;
}

void meshPagedName(char* outFileName, char* inFileName)
{
    char  fileName[1024];
    char* token;

    outFileName[0] = '\0';
    strcpy(fileName, inFileName);

    token = strtok(fileName, "\\/");
    while (token != NULL)
    {
        if (meshStringContainsPeriod(token))
        {
            token[strlen(token) - 3] = 'p';
            strcat(outFileName, token);
        }
        else
        {
            strcat(outFileName, token);
#ifdef _WIN32
            strcat(outFileName, "\\");
#else
            strcat(outFileName, "/");
#endif
        }

        token = strtok(NULL, "\\/");
    }
}

/*-----------------------------------------------------------------------------
    Name        : meshPagedVersionExists
    Description : See if the paged version (.peo) of a file exists.
    Inputs      : fileName - file to check existence of paged version
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 meshPagedVersionExists(char* fileName)
{
    char pagedName[1024];

    meshPagedName(pagedName, fileName);
    return fileExists(pagedName, 0);
}

/*-----------------------------------------------------------------------------
    Name        : meshObjectFixupPacoUV
    Description : fixup UV seaming on a particular polygon object
    Inputs      : object - the polygonobject to fixup or un-fixup
                  on - to fixup or not fixup
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void meshObjectFixupPacoUV(polygonobject* object, sdword on)
{
    polyentry* polygon;
    sdword iPoly;
    sdword width, height;
    real32 s, t;

    for (iPoly = 0, polygon = object->pPolygonList; iPoly < object->nPolygons; iPoly++, polygon++)
    {
        if (polygon->reserved[0] > 1)
        {
            //will be 0 or 1 if adjust data exists
            continue;
        }

        //isolate width, height nibbles
        width  = (polygon->reserved[1] & 0xf0) >> 4;
        height =  polygon->reserved[1] & 0x0f;
        //convert exp2 to number
        width  = 1 << width;
        height = 1 << height;
        //define correction factors
        s = 0.6f / (real32)width;
        t = 0.6f / (real32)height;

        if (on)
        {
            //want to fixup
            if (bitTest(polygon->flags, PAC_FIXED))
            {
                //already fixed up
                continue;
            }
            else
            {
                //not fixed up yet
                bitSet(polygon->flags, PAC_FIXED);
            }
        }
        else
        {
            //want to unfixup
            if (bitTest(polygon->flags, PAC_FIXED))
            {
                //currently fixed up
                bitClear(polygon->flags, PAC_FIXED);
                s = -s;
                t = -t;
            }
            else
            {
                //already unfixed
                continue;
            }
        }

        //convert uv coords that need converting
        //0
        if (bitTest(polygon->flags, PAC_S0_0))
        {
            polygon->s0 += s;
        }
        if (bitTest(polygon->flags, PAC_S0_1))
        {
            polygon->s0 -= s;
        }
        if (bitTest(polygon->flags, PAC_T0_0))
        {
            polygon->t0 += t;
        }
        if (bitTest(polygon->flags, PAC_T0_1))
        {
            polygon->t0 -= t;
        }
        //1
        if (bitTest(polygon->flags, PAC_S1_0))
        {
            polygon->s1 += s;
        }
        if (bitTest(polygon->flags, PAC_S1_1))
        {
            polygon->s1 -= s;
        }
        if (bitTest(polygon->flags, PAC_T1_0))
        {
            polygon->t1 += t;
        }
        if (bitTest(polygon->flags, PAC_T1_1))
        {
            polygon->t1 -= t;
        }
        //2
        if (bitTest(polygon->flags, PAC_S2_0))
        {
            polygon->s2 += s;
        }
        if (bitTest(polygon->flags, PAC_S2_1))
        {
            polygon->s2 -= s;
        }
        if (bitTest(polygon->flags, PAC_T2_0))
        {
            polygon->t2 += t;
        }
        if (bitTest(polygon->flags, PAC_T2_1))
        {
            polygon->t2 -= t;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : meshFixupPacoUV
    Description : apply UV munging to fixup texture seaming to a particular mesh
    Inputs      : mesh - the mesh
                  on - to fixup or not fixup
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void meshFixupPacoUV(meshdata* mesh, sdword on)
{
    sdword index;
    polygonobject* object;

    if (mesh->localSize == 0xFFFFFFFF)
    {
        //old-style mesh, no fixup info
        return;
    }

    if (on)
    {
        if (mesh->localSize > 0)
        {
            //already fixed up
            return;
        }
        else
        {
            mesh->localSize = 1;
        }
    }
    else
    {
        if (mesh->localSize == 0)
        {
            //already un-fixed
            return;
        }
        else
        {
            mesh->localSize = 0;
        }
    }

    for (index = 0, object = &mesh->object[0]; index < mesh->nPolygonObjects; index++, object++)
    {
        meshObjectFixupPacoUV(object, on);
    }
}


#ifdef HW_PTR_64
/*-----------------------------------------------------------------------------
    Name        : meshConvertOffset64 (GoK 64-bit port helper)
    Description : CAT: maps a file offset that points behind the material table (vertex, vertex-info, normal and
                  polygon lists, object, material and texture names) into the 64-bit image built by
                  meshLoadAndConvertTo64Bit, where that part of the file is copied verbatim to blobStart.
    Inputs      : fileOffset - offset as stored in the file; payloadStart - first byte behind the material table;
                  blobStart - where that byte lands in the new image; fileLength - length of the file
    Return      : the offset in the new image
----------------------------------------------------------------------------*/
static memsize meshConvertOffset64(udword fileOffset, udword payloadStart, memsize blobStart, sdword fileLength,
                                   char *fileName, char *what)
{
    if (fileOffset < payloadStart || fileOffset > (udword)fileLength)
    {
        dbgFatalf(DBG_Loc, "meshLoad: %s offset 0x%x in %s is outside the data area 0x%x..0x%x",
                  what, fileOffset, fileName, payloadStart, fileLength);
    }
    return blobStart + (fileOffset - payloadStart);
}

/*-----------------------------------------------------------------------------
    Name        : meshLoadAndConvertTo64Bit (GoK 64-bit port helper, not in the exe)
    Description : Loads a .geo/.peo file and rebuilds it with 64-bit pointer-sized records, so meshLoad can fix it
                  up as the 32-bit game fixes up the file image in place.
                  CAT: rewritten for Cataclysm's layout (meshLoad 0x004aec90, docs/formats/mesh.md):
                  - polygon objects are read at a stride of 0x74 (polygonobject_disk; HW1 0x70) and carry the new
                    per-vertex info list (+0x1C, pVertexInfo);
                  - everything behind the material table (the vertex, vertex-info, normal and polygon lists and the
                    name strings) is copied as ONE verbatim block and every offset into it is translated. Duplicate
                    objects therefore keep sharing their arrays as in the exe (400 of the 1,729 shipped files share
                    at least one array; GoK's converter copied each object's arrays separately into a buffer of
                    3 * file length, and dropped the material names);
                  - the image size is computed, not guessed;
                  - a file whose table does not have the 0x74 stride (an HW1 0x402 mesh) is a fatal error with the
                    file name, where the exe would silently misparse it.
    Inputs      : fileName - file to load
    Outputs     : *loadAddress - the new image (memAlloc'ed): GeoFileHeader, polygonobject[n], materialentry[m],
                  then the verbatim data block
    Return      : length of the new image
----------------------------------------------------------------------------*/
sdword meshLoadAndConvertTo64Bit(char *fileName, void** loadAddress)
{
    ubyte *oldbase = NULL;
    ubyte *newbase;
    GeoFileHeader_disk *oldHeader;
    GeoFileHeader *newHeader;
    polygonobject_disk *oldpolobj;
    polygonobject *newpolobj;
    materialentry_disk *oldmaterial;
    materialentry *newmaterial;
    sdword oldLength, i, nObjects, nMaterials;
    udword oldObjectsEnd, oldMaterialsEnd;
    memsize newObjects, newMaterials, blobStart, newLength;

    oldLength = fileLoadAlloc(fileName, (void **)&oldbase, NonVolatile);
    if (oldLength < (sdword)sizeof(GeoFileHeader_disk))
    {
        dbgFatalf(DBG_Loc, "meshLoad: %s is too short for a mesh header (%d bytes)", fileName, oldLength);
    }
    oldHeader = (GeoFileHeader_disk *)oldbase;
    nObjects = (sdword)oldHeader->nPolygonObjects;
    nMaterials = (sdword)(oldHeader->nPublicMaterials + oldHeader->nLocalMaterials);
    oldObjectsEnd = sizeof(GeoFileHeader_disk) + (udword)nObjects * sizeof(polygonobject_disk);
    oldMaterialsEnd = oldHeader->oPublicMaterial + (udword)nMaterials * sizeof(materialentry_disk);
    // CAT: the object table ends where the material table starts, at 0x44 + n * 0x74 in all 1,729 shipped files
    if (nObjects < 0 || nMaterials < 0 || oldHeader->oPublicMaterial != oldObjectsEnd ||
        oldMaterialsEnd > (udword)oldLength)
    {
        dbgFatalf(DBG_Loc, "meshLoad: %s does not have Cataclysm's 0x74-byte polygon objects (%d objects, materials at 0x%x, expected 0x%x)",
                  fileName, nObjects, oldHeader->oPublicMaterial, oldObjectsEnd);
    }

    newObjects = sizeof(GeoFileHeader);
    newMaterials = newObjects + (memsize)nObjects * sizeof(polygonobject);
    blobStart = newMaterials + (memsize)nMaterials * sizeof(materialentry);
    newLength = blobStart + (memsize)(oldLength - oldMaterialsEnd);

    newbase = memAlloc(newLength, "mesh 64bit conversion", 0);
    memset(newbase, 0, newLength);
    newHeader = (GeoFileHeader *)newbase;

    memcpy(newHeader->identifier, oldHeader->identifier, MESH_FileIDLength);
    newHeader->version = oldHeader->version;
    newHeader->pName = (oldHeader->pName == 0) ? NULL :
        (char *)meshConvertOffset64(oldHeader->pName, oldMaterialsEnd, blobStart, oldLength, fileName, "mesh name");
    newHeader->__obsolete = oldHeader->fileSize;
    newHeader->localSize = oldHeader->localSize;
    newHeader->nPublicMaterials = oldHeader->nPublicMaterials;
    newHeader->nLocalMaterials = oldHeader->nLocalMaterials;
    newHeader->oPublicMaterial = newMaterials;
    // CAT: the exe keeps both table pointers (publicMaterial = oPublicMaterial, localMaterial = oLocalMaterial)
    if (oldHeader->oLocalMaterial < oldHeader->oPublicMaterial || oldHeader->oLocalMaterial > oldMaterialsEnd ||
        (oldHeader->oLocalMaterial - oldHeader->oPublicMaterial) % sizeof(materialentry_disk) != 0)
    {
        dbgFatalf(DBG_Loc, "meshLoad: %s has a local material table at 0x%x", fileName, oldHeader->oLocalMaterial);
    }
    newHeader->oLocalMaterial = newMaterials +
        (oldHeader->oLocalMaterial - oldHeader->oPublicMaterial) / sizeof(materialentry_disk) * sizeof(materialentry);
    newHeader->nPolygonObjects = oldHeader->nPolygonObjects;
    memcpy(newHeader->reserved, oldHeader->reserved, sizeof(newHeader->reserved));

    // polygon objects: pointers become offsets into the new image, meshLoad adds the base
    oldpolobj = (polygonobject_disk *)(oldbase + sizeof(GeoFileHeader_disk));
    newpolobj = (polygonobject *)(newbase + newObjects);
    for (i = 0; i < nObjects; i++)
    {
        newpolobj[i].pName = (oldpolobj[i].pName == 0) ? NULL :
            (char *)meshConvertOffset64(oldpolobj[i].pName, oldMaterialsEnd, blobStart, oldLength, fileName, "object name");
        newpolobj[i].flags = oldpolobj[i].flags;
        newpolobj[i].iObject = oldpolobj[i].iObject;
        newpolobj[i].nameCRC = oldpolobj[i].nameCRC;
        newpolobj[i].nVertices = oldpolobj[i].nVertices;
        newpolobj[i].nFaceNormals = oldpolobj[i].nFaceNormals;
        newpolobj[i].nVertexNormals = oldpolobj[i].nVertexNormals;
        newpolobj[i].nPolygons = oldpolobj[i].nPolygons;
        // CAT: the four list pointers are fixed up unconditionally, as in meshLoad 0x004aec90
        newpolobj[i].pVertexList = (vertexentry *)meshConvertOffset64(oldpolobj[i].pVertexList, oldMaterialsEnd,
            blobStart, oldLength, fileName, "vertex list");
        newpolobj[i].pVertexInfo = (vertexinfo *)meshConvertOffset64(oldpolobj[i].pVertexInfo, oldMaterialsEnd,
            blobStart, oldLength, fileName, "vertex info list");
        newpolobj[i].pNormalList = (normalentry *)meshConvertOffset64(oldpolobj[i].pNormalList, oldMaterialsEnd,
            blobStart, oldLength, fileName, "normal list");
        newpolobj[i].pPolygonList = (polyentry *)meshConvertOffset64(oldpolobj[i].pPolygonList, oldMaterialsEnd,
            blobStart, oldLength, fileName, "polygon list");
        if (oldpolobj[i].nVertices < 0 || oldpolobj[i].nFaceNormals < 0 || oldpolobj[i].nVertexNormals < 0 ||
            oldpolobj[i].nPolygons < 0 ||
            (memsize)oldpolobj[i].pVertexList + (memsize)oldpolobj[i].nVertices * sizeof(vertexentry) > (memsize)oldLength ||
            (memsize)oldpolobj[i].pVertexInfo + (memsize)oldpolobj[i].nVertices * sizeof(vertexinfo) > (memsize)oldLength ||
            (memsize)oldpolobj[i].pNormalList + (memsize)(oldpolobj[i].nFaceNormals + oldpolobj[i].nVertexNormals) * sizeof(normalentry) > (memsize)oldLength ||
            (memsize)oldpolobj[i].pPolygonList + (memsize)oldpolobj[i].nPolygons * sizeof(polyentry) > (memsize)oldLength)
        {
            dbgFatalf(DBG_Loc, "meshLoad: object %d of %s has a list past the end of the file", i, fileName);
        }
        // hierarchy links: file offsets of other polygon objects
#define MESH_CONVERT_LINK(field)                                                                            \
        if (oldpolobj[i].field != 0)                                                                        \
        {                                                                                                   \
            if (oldpolobj[i].field < sizeof(GeoFileHeader_disk) || oldpolobj[i].field >= oldObjectsEnd ||   \
                (oldpolobj[i].field - sizeof(GeoFileHeader_disk)) % sizeof(polygonobject_disk) != 0)        \
            {                                                                                               \
                dbgFatalf(DBG_Loc, "meshLoad: object %d of %s has a bad " #field " link 0x%x", i, fileName, \
                          oldpolobj[i].field);                                                              \
            }                                                                                               \
            newpolobj[i].field = (polygonobject *)(newObjects + (oldpolobj[i].field - sizeof(GeoFileHeader_disk)) / \
                sizeof(polygonobject_disk) * sizeof(polygonobject));                                        \
        }
        MESH_CONVERT_LINK(pMother)
        MESH_CONVERT_LINK(pDaughter)
        MESH_CONVERT_LINK(pSister)
#undef MESH_CONVERT_LINK
        newpolobj[i].localMatrix = oldpolobj[i].localMatrix;
    }

    // materials: same order; names and texture names become offsets into the data block
    oldmaterial = (materialentry_disk *)(oldbase + oldHeader->oPublicMaterial);
    newmaterial = (materialentry *)(newbase + newMaterials);
    for (i = 0; i < nMaterials; i++)
    {
        newmaterial[i].pName = (oldmaterial[i].pName == 0) ? NULL :
            (char *)meshConvertOffset64(oldmaterial[i].pName, oldMaterialsEnd, blobStart, oldLength, fileName, "material name");
        newmaterial[i].ambient = oldmaterial[i].ambient;
        newmaterial[i].diffuse = oldmaterial[i].diffuse;
        newmaterial[i].specular = oldmaterial[i].specular;
        newmaterial[i].kAlpha = oldmaterial[i].kAlpha;
        newmaterial[i].texture = (oldmaterial[i].texture == 0) ? NULL :
            (trhandle *)meshConvertOffset64(oldmaterial[i].texture, oldMaterialsEnd, blobStart, oldLength, fileName, "texture name");
        newmaterial[i].flags = oldmaterial[i].flags;
        newmaterial[i].nFullAmbient = oldmaterial[i].nFullAmbient;
        newmaterial[i].bTexturesRegistered = oldmaterial[i].bTexturesRegistered;
        newmaterial[i].textureNameSave = NULL;              // set by meshLoad from the texture name, as in the exe
    }

    // the data block, verbatim
    memcpy(newbase + blobStart, oldbase + oldMaterialsEnd, (size_t)(oldLength - oldMaterialsEnd));
    memFree(oldbase);

    *loadAddress = newbase;
    return (sdword)newLength;
}
#endif


/*-----------------------------------------------------------------------------
    Name        : meshLoad
    Description : Loads in a mesh file.
    Inputs      : fileName - name of file to load
    Outputs     : Allocates and loads data returned in meshdata structure.
    Return      : Pointer to allocated and fixed-up meshdata structure.
----------------------------------------------------------------------------*/
meshdata *meshLoad(char *inFileName)
{
    GeoFileHeader header;
    meshdata *mesh;
    sdword index;
    memsize offset;
    polygonobject *object;
    char fullName[FL_Path];
    sdword fileLength;
    trhandle *handles;
    ubyte *loadAddress;

#if FIX_ENDIAN
    sdword i;
	udword *vertsDone  = NULL;
	udword *normsDone  = NULL;
	udword *polysDone  = NULL;
	int found          = -1;
	int vertsDoneCount = 0;
	int normsDoneCount = 0;
	int polysDoneCount = 0;
#endif

    bool32 allowPacking;
    bool32 newFormat;

    char  pagedName[1024];
    char* fileName;

    allowPacking = mainAllowPacking;

    if (allowPacking && meshPagedVersionExists(inFileName))
    {
        meshPagedName(pagedName, inFileName);
        fileName = pagedName;
    }
    else
    {
        fileName = inFileName;
    }

#ifdef HW_PTR_64
    fileLength = meshLoadAndConvertTo64Bit(fileName, (void **)&loadAddress);
#else
    fileLength = fileLoadAlloc(fileName, (void **)&loadAddress, NonVolatile);     //load in the file
#endif
    
    memcpy(&header, loadAddress, sizeof(GeoFileHeader));

#if FIX_ENDIAN
	header.version          = FIX_ENDIAN_INT_32( header.version );
	header.pName            = ( char *)FIX_ENDIAN_INT_32( ( udword )header.pName );
	header.localSize        = FIX_ENDIAN_INT_32( header.localSize );
	header.nPublicMaterials = FIX_ENDIAN_INT_32( header.nPublicMaterials );
	header.nLocalMaterials  = FIX_ENDIAN_INT_32( header.nLocalMaterials );
	header.oPublicMaterial  = FIX_ENDIAN_INT_32( header.oPublicMaterial );
	header.oLocalMaterial   = FIX_ENDIAN_INT_32( header.oLocalMaterial );
	header.nPolygonObjects  = FIX_ENDIAN_INT_32( header.nPolygonObjects );

	vertsDone = ( udword *)malloc( header.nPolygonObjects * sizeof( udword ) );
	normsDone = ( udword *)malloc( header.nPolygonObjects * sizeof( udword ) );
	polysDone = ( udword *)malloc( header.nPolygonObjects * sizeof( udword ) );

	memset( vertsDone, 0, header.nPolygonObjects * sizeof( udword ) );
	memset( normsDone, 0, header.nPolygonObjects * sizeof( udword ) );
	memset( polysDone, 0, header.nPolygonObjects * sizeof( udword ) );
#endif

#if MESH_ERROR_CHECKING
    if (strncmp(header.identifier, MESH_FileID, MESH_FileIDLength))//verify ident. string
    {
        if (strncmp(header.identifier, MESH_FileID2, MESH_FileIDLength))
        {
            dbgFatalf(DBG_Loc, "meshLoad: Invalid ident string in %s.  Expected %s, found %s", fileName, MESH_FileID, header.identifier);
        }
    }
    // CAT: no version check. Cataclysm's meshLoad 0x004aec90 never reads the version dword; its files say 0x500
    // (1,727 files) or 0x402 (2 files, already in the 0x74 layout), so the stamp says nothing about the layout.
    // The 64-bit conversion checks the 0x74 object stride instead.
#endif

    if (strncmp(header.identifier, MESH_FileID2, MESH_FileIDLength) == 0)
    {
        newFormat = TRUE;
    }
    else
    {
        newFormat = FALSE;
    }

    mesh = memAlloc(fileLength - sizeof(header) + sizeof(meshdata) -   //allocate the memory for file
                    sizeof(polygonobject), fileName, NonVolatile);
    memNameSetLong((memcookie*)((ubyte *)mesh - sizeof(memcookie)), fileName);
    //now that it is all loaded in, all pointers need to be fixed up
    offset = (memsize)mesh - sizeof(GeoFileHeader) +
        sizeof(meshdata) - sizeof(polygonobject);           //!!! add size of materials when they're in???
    if (newFormat)
    {
        mesh->localSize = 0;
    }
    else
    {
        mesh->localSize = UDWORD_Max;
    }
    mesh->nPublicMaterials = header.nPublicMaterials;       //set other header members
    mesh->nLocalMaterials = header.nLocalMaterials;
    mesh->publicMaterial = (materialentry *)(header.oPublicMaterial + offset);
    mesh->localMaterial = (materialentry *)(header.oLocalMaterial + offset);
    mesh->nPolygonObjects = header.nPolygonObjects;
                                                            //read remainder of the file
#if MESH_RETAIN_FILENAMES
    mesh->fileName = memStringDupe(fileName);
#endif
    
    memcpy(&mesh->object[0], loadAddress + sizeof(GeoFileHeader), fileLength - sizeof(header));
    memFree(loadAddress);

    mesh->name = header.pName + offset;                     //set name pointer
    //fix up the polgon objects
    for (index = 0; index < mesh->nPolygonObjects; index++)
    {
        object = &mesh->object[index];                      //get pointer to poly object

#if FIX_ENDIAN
		if( !object ) continue;

		object->pName          = ( char *)FIX_ENDIAN_INT_32( ( udword )object->pName );
		object->nameCRC        = FIX_ENDIAN_INT_16( object->nameCRC );
		object->nVertices      = FIX_ENDIAN_INT_32( object->nVertices );
		object->nFaceNormals   = FIX_ENDIAN_INT_32( object->nFaceNormals );
		object->nVertexNormals = FIX_ENDIAN_INT_32( object->nVertexNormals );
		object->nPolygons      = FIX_ENDIAN_INT_32( object->nPolygons );
		object->pVertexList    = ( vertexentry *)FIX_ENDIAN_INT_32( ( udword )object->pVertexList );
		object->pVertexInfo    = ( vertexinfo *)FIX_ENDIAN_INT_32( ( udword )object->pVertexInfo );    // CAT: new list (meshLoad 0x004aec90)
		object->pNormalList    = ( normalentry *)FIX_ENDIAN_INT_32( ( udword )object->pNormalList );
		object->pPolygonList   = ( polyentry *)FIX_ENDIAN_INT_32( ( udword )object->pPolygonList );
		object->pMother        = ( polygonobject *)FIX_ENDIAN_INT_32( ( udword )object->pMother );
		object->pDaughter      = ( polygonobject *)FIX_ENDIAN_INT_32( ( udword )object->pDaughter );
		object->pSister        = ( polygonobject *)FIX_ENDIAN_INT_32( ( udword )object->pSister );

        // anonymous block so I can declare mptr with limited scope and not have
        // a plain C compiler complain
		{
            real32 *mptr = ( real32 *)&object->localMatrix;
		    for( i=0; i<16; i++ )
            {
			    mptr[i] = FIX_ENDIAN_FLOAT_32( mptr[i] );
            }
        }
#endif

        //object->pName += offset;                          //!!! is there a name
        object->pVertexList = (vertexentry *)(offset + (ubyte *)object->pVertexList);             //fixup vertex list pointer
        object->pVertexInfo = (vertexinfo *)(offset + (ubyte *)object->pVertexInfo);              // CAT: the new per-vertex info list, fixed up unconditionally (meshLoad 0x004aec90)
        object->pNormalList = (normalentry *)(offset + (ubyte *)object->pNormalList);             //fixup vertex list pointer
        object->pPolygonList = (polyentry *)(offset + (ubyte *)object->pPolygonList);            //fixup vertex list pointer
        if (object->pMother != NULL)
        {
            object->pMother = (polygonobject *)(offset + (ubyte *)object->pMother);
        }
        if (object->pDaughter != NULL)
        {
            object->pDaughter = (polygonobject *)(offset + (ubyte *)object->pDaughter);
        }
        if (object->pSister != NULL)
        {
            object->pSister = (polygonobject *)(offset + (ubyte *)object->pSister);
        }
        if (object->pName != NULL)
        {
            object->pName = offset + object->pName;               //fix up name
        }
        object->iObject = index;
        object->nameCRC = crc16Compute((ubyte*)object->pName, strlen(object->pName));

#if FIX_ENDIAN
		found = -1;
		for( i=0; i<mesh->nPolygonObjects; i++ )
		{
			if( vertsDone[i] == object->pVertexList )
			{
				found = i;
				break;
			}
		}

		if( found == -1 )
		{
			for( i=0; i<object->nVertices; i++ )
			{
				object->pVertexList[i].x = FIX_ENDIAN_FLOAT_32( object->pVertexList[i].x );
				object->pVertexList[i].y = FIX_ENDIAN_FLOAT_32( object->pVertexList[i].y );
				object->pVertexList[i].z = FIX_ENDIAN_FLOAT_32( object->pVertexList[i].z );
				object->pVertexList[i].iVertexNormal = FIX_ENDIAN_INT_32( object->pVertexList[i].iVertexNormal );
			}

			vertsDone[vertsDoneCount++] = object->pVertexList;
		}

		found = -1;
		for( i=0; i<mesh->nPolygonObjects; i++ )
		{
			if( normsDone[i] == object->pNormalList )
			{
				found = i;
				break;
			}
		}

		if( found == -1 )
		{
			for( i=0; i<object->nFaceNormals + object->nVertexNormals; i++ )
			{
				object->pNormalList[i].x = FIX_ENDIAN_FLOAT_32( object->pNormalList[i].x );
				object->pNormalList[i].y = FIX_ENDIAN_FLOAT_32( object->pNormalList[i].y );
				object->pNormalList[i].z = FIX_ENDIAN_FLOAT_32( object->pNormalList[i].z );
			}

			normsDone[normsDoneCount++] = object->pNormalList;
		}

		found = -1;
		for( i=0; i<mesh->nPolygonObjects; i++ )
		{
			if( polysDone[i] == object->pPolygonList )
			{
				found = i;
				break;
			}
		}

		if( found == -1 )
		{
			for( i=0; i<object->nPolygons; i++ )
			{
				object->pPolygonList[i].iFaceNormal = FIX_ENDIAN_INT_32( object->pPolygonList[i].iFaceNormal );
				object->pPolygonList[i].iV0 = FIX_ENDIAN_INT_16( object->pPolygonList[i].iV0 );
				object->pPolygonList[i].iV1 = FIX_ENDIAN_INT_16( object->pPolygonList[i].iV1 );
				object->pPolygonList[i].iV2 = FIX_ENDIAN_INT_16( object->pPolygonList[i].iV2 );
				object->pPolygonList[i].iMaterial = FIX_ENDIAN_INT_16( object->pPolygonList[i].iMaterial );
				object->pPolygonList[i].s0 = FIX_ENDIAN_FLOAT_32( object->pPolygonList[i].s0 );
				object->pPolygonList[i].t0 = FIX_ENDIAN_FLOAT_32( object->pPolygonList[i].t0 );
				object->pPolygonList[i].s1 = FIX_ENDIAN_FLOAT_32( object->pPolygonList[i].s1 );
				object->pPolygonList[i].t1 = FIX_ENDIAN_FLOAT_32( object->pPolygonList[i].t1 );
				object->pPolygonList[i].s2 = FIX_ENDIAN_FLOAT_32( object->pPolygonList[i].s2 );
				object->pPolygonList[i].t2 = FIX_ENDIAN_FLOAT_32( object->pPolygonList[i].t2 );
				object->pPolygonList[i].flags = FIX_ENDIAN_INT_16( object->pPolygonList[i].flags );
			}

			polysDone[polysDoneCount++] = object->pPolygonList;
		}
    }

	free( vertsDone );
	free( normsDone );
	free( polysDone );

	for (index = 0; index < mesh->nLocalMaterials + mesh->nPublicMaterials; index++)
	{
		mesh->localMaterial[index].pName = ( char *)FIX_ENDIAN_INT_32( ( udword )mesh->localMaterial[index].pName );
//		mesh->localMaterial[index].ambient = FIX_ENDIAN_INT_32( mesh->localMaterial[index].ambient );
//		mesh->localMaterial[index].diffuse = FIX_ENDIAN_INT_32( mesh->localMaterial[index].diffuse );
//		mesh->localMaterial[index].specular = FIX_ENDIAN_INT_32( mesh->localMaterial[index].specular );
		mesh->localMaterial[index].kAlpha = FIX_ENDIAN_FLOAT_32( mesh->localMaterial[index].kAlpha );
		mesh->localMaterial[index].texture = FIX_ENDIAN_INT_32( mesh->localMaterial[index].texture );
		mesh->localMaterial[index].flags = FIX_ENDIAN_INT_16( mesh->localMaterial[index].flags );
		mesh->localMaterial[index].textureNameSave = ( char *)FIX_ENDIAN_INT_32( ( udword )mesh->localMaterial[index].textureNameSave );
#endif // FIX_ENDIAN

	} // FIX_ENDIAN:    end of for(index < mesh->nLocalMaterials...) loop
      // everyone else: end of for(index < mesh->nPolygonObjects)    loop

    if (allowPacking && mainOnlyPacking && (fileName != pagedName))
    {
        for (index = 0; index < mesh->nLocalMaterials + mesh->nPublicMaterials; index++)
        {
            if (mesh->localMaterial[index].pName != NULL)
            {
                mesh->localMaterial[index].pName += offset;
            }
            mesh->localMaterial[index].bTexturesRegistered = FALSE;
            mesh->localMaterial[index].texture = 0;
        }

        return(mesh);
    }

    for (index = 0; index < mesh->nLocalMaterials + mesh->nPublicMaterials; index++)
    {                                                       //for all materials
        if (mesh->localMaterial[index].pName != NULL)
        {
            mesh->localMaterial[index].pName += offset;
        }
        mesh->localMaterial[index].bTexturesRegistered = FALSE;
        if (mesh->localMaterial[index].texture != 0)
        {                                                   //if there is a texture
            mesh->localMaterial[index].texture = (trhandle *)(offset + (char *)(mesh->localMaterial[index].texture));//fix up texture name pointer
            mesh->localMaterial[index].textureNameSave = (char *)mesh->localMaterial[index].texture;
            meshTextureNameToPath(fullName, fileName, (char *)mesh->localMaterial[index].texture);
            handles = meshTextureRegisterAllPlayers(fullName, mesh);
            if (handles != NULL)
            {                                               //if anything was registered
                mesh->localMaterial[index].texture = handles;
                mesh->localMaterial[index].bTexturesRegistered = TRUE;
            }
        }
/*
        else
        {
            mesh->localMaterial[index].texture = TR_Invalid;
        }
*/
    }

    return(mesh);
}

/*-----------------------------------------------------------------------------
    Name        : meshObjectFixupUV
    Description : nudges UV coordinates inwards to eliminate filtered oversampling errors
    Inputs      : object - polygonobject whose UVs are to be modified
                  materials - list of materials on the object
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void meshObjectFixupUV(polygonobject* object, materialentry* materials)
{
    sdword iPoly;
    polyentry* polygon;
    sdword currentMaterial;
    materialentry* material;
    trhandle handle;
    texreg* reg;
    real32 zero, one;

    for (iPoly = 0, polygon = object->pPolygonList; iPoly < object->nPolygons; iPoly++, polygon++)
    {
        currentMaterial = polygon->iMaterial;
        material = &materials[currentMaterial];

        if (material->bTexturesRegistered && (material->texture != 0))
        {
            handle = ((trhandle*)material->texture)[0/*iColorScheme*/];
//!!!            dbgAssertOrIgnore(handle != TR_InvalidHandle);
            if (handle == TR_InvalidHandle)
            {
                continue;
            }
            reg = trStructureGet(handle);

            zero = 0.51f / reg->scaledWidth;
            one = 1.0f - zero;

            if (polygon->s0 == 0.0f) polygon->s0 = zero;
            if (polygon->s0 == 1.0f) polygon->s0 = one;
            if (polygon->t0 == 0.0f) polygon->t0 = zero;
            if (polygon->t0 == 1.0f) polygon->t0 = one;

            if (polygon->s1 == 0.0f) polygon->s1 = zero;
            if (polygon->s1 == 1.0f) polygon->s1 = one;
            if (polygon->t1 == 0.0f) polygon->t1 = zero;
            if (polygon->t1 == 1.0f) polygon->t1 = one;

            if (polygon->s2 == 0.0f) polygon->s2 = zero;
            if (polygon->s2 == 1.0f) polygon->s2 = one;
            if (polygon->t2 == 0.0f) polygon->t2 = zero;
            if (polygon->t2 == 1.0f) polygon->t2 = one;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : meshFixupUV
    Description : nudges UV coordinates inwards to eliminate filtered oversampling errors
    Inputs      : mesh - the mesh to fixup
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void meshFixupUV(meshdata* mesh)
{
    sdword index;
    polygonobject* object;

    for (index = 0, object = &mesh->object[0]; index < mesh->nPolygonObjects; index++, object++)
    {
        meshObjectFixupUV(object, mesh->localMaterial);
    }
}

/*-----------------------------------------------------------------------------
    Name        : meshRecolorize
    Description : Recolorize all the textures of a mesh based upon new colors
                    that may have been picked.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void meshRecolorize(meshdata *mesh)
{
    sdword index, j, k;
    trhandle *handles, validHandle;
    StaticInfo *info;
    char fullName[FL_Path];

    info = meshNameToStaticInfo(mesh->fileName);
    if (info == NULL)
    {
        return;
    }
    for (index = 0; index < mesh->nLocalMaterials + mesh->nPublicMaterials; index++)
    {                                                       //for all materials in mesh
        //3 states of this member:TR_Invalid, pointer to list of texture handles or 0 (no texture)
        if (mesh->localMaterial[index].texture != (trhandle*)TR_Invalid &&
            mesh->localMaterial[index].texture != 0) { // if textured material
            if (!mesh->localMaterial[index].bTexturesRegistered)
            {                                               //if textures were never registered properly
                meshTextureNameToPath(fullName, mesh->fileName, (char *)mesh->localMaterial[index].texture);
                handles = meshTextureRegisterAllPlayers(fullName, mesh);
                if (handles != NULL)
                {                                               //if anything was registered
                    mesh->localMaterial[index].texture = handles;
                    mesh->localMaterial[index].bTexturesRegistered = TRUE;
                }
            }
            else
            {                                               //textures were previously registered properly
                handles = (trhandle *)mesh->localMaterial[index].texture;
                validHandle = TR_Invalid;
                //4 passes for all players
                //first pass: see if any texture colors are way out of whack
                for (j = 0; j < MAX_MULTIPLAYER_PLAYERS; j++)
                {
                    if (handles[j] != TR_Invalid)
                    {                                       //if this texture allocated
    				    for (k = 0; k < MAX_MULTIPLAYER_PLAYERS; k++)
    					{
                            if (((ShipStaticInfo *)info)->shipOrDerelict.teamColor[k])
                            {                               //if this team color will be loaded
                                if (k != j && trColorsEqual(&teColorSchemes[k].textureColor, trIndex(handles[j])) == trPaletteIndex(handles[j]))
                                {                           //if this handle's color is in some other team's desired color
                				    for (k = 0; k < MAX_MULTIPLAYER_PLAYERS; k++)
                					{                       //delete all handles; this texture will have to be loaded from scratch
                                        if (handles[k] != TR_Invalid)
                                        {
                                            trRegisterRemoval(handles[k]);
                                            handles[k] = TR_Invalid;
                                        }
                                    }
                                    goto doneDeletingAllOldTextures;//break-break, skip next 2 passes
                                }
                            }
                        }
                    }
                }

                //second pass: find a valid handle
                for (j = 0; j < MAX_MULTIPLAYER_PLAYERS; j++)
                {                                           //find a valid handle
                    if (handles[j] != TR_Invalid)
                    {
                        validHandle = handles[j];
                        break;
                    }
                }

                //third pass: removals of unused color schemes
                for (j = 0; j < MAX_MULTIPLAYER_PLAYERS; j++)
                {                                           //for all players
                    if (!((ShipStaticInfo *)info)->shipOrDerelict.teamColor[j])
                    {                                       //if this player does not need this mesh
                        if (handles[j] != TR_Invalid)
                        {                                   //this texture no longer needed
                            trRegisterRemoval(handles[j]);  //delete this version of the texture (without reloading) ...
                            handles[j] = TR_Invalid;
                        }
                    }
                }
doneDeletingAllOldTextures:

                //fourth pass: additions and updates of color schemes
                for (j = 0; j < MAX_MULTIPLAYER_PLAYERS; j++)
                {                                           //for all players
                    if (((ShipStaticInfo *)info)->shipOrDerelict.teamColor[j])
                    {                                       //if this player needs this mesh
                        if (handles[j] != TR_Invalid)
                        {                                   //if texture was loaded properly before
                            trTextureColorsUpdate(handles[j],//update the colors
                                &teColorSchemes[j].textureColor);
                        }
                        else
                        {                                   //this texture was never loaded properly
                            //register the texture with a new color
                            if (validHandle == TR_Invalid)
                            {                               //if no valid handles (they've all be unregistered)
                                meshTextureNameToPath(fullName, mesh->fileName, mesh->localMaterial[index].textureNameSave);
                                validHandle = handles[j] = trTextureRegister(fullName, &teColorSchemes[j].textureColor, mesh, TRUE);  // CAT: 4th arg 1 (meshRecolorize 0x004af3a0)
                            }
                            else
                            {                               //else a valid handle aleready exists for this texture
                                handles[j] = trRegisterAddition(validHandle,
                                    &teColorSchemes[j].textureColor);
                            }
                        }
                    }
#if MESH_ERROR_CHECKING
                    else if (handles[j] != TR_Invalid)
                    {                                       //this texture no longer needed
                        dbgFatalf(DBG_Loc, "Improperly removed texture for player #%d", j);
                    }
#endif
                }
/*
#if MESH_ERROR_CHECKING
	            //make sure no duplicate handles
		        for (l = 0; l < MAX_MULTIPLAYER_PLAYERS; l++)
			    {
				    for (k = 0; k < MAX_MULTIPLAYER_PLAYERS; k++)
					{                                           //find a valid handle
						if (k != l && handles[l] != TR_Invalid && handles[l] != 0)
	                    {
		                    if (handles[l] == handles[k])
						    {
					            dbgFatalf(DBG_Loc, "handles[%d] == handles[%d] in iteration %d.", l, k, j);
				            }
			            }
		            }
	            }
#endif
*/
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : meshFree
    Description : Free up the specified mesh.
    Inputs      : mesh - pointer to mesh to be freed.
    Outputs     :
    Return      : void
    Note        : does not free up any instances of thie mesh or any textures
                    bound to it.
----------------------------------------------------------------------------*/
void meshFree(meshdata *mesh)
{
    sdword index, j;

    for (index = 0; index < mesh->nLocalMaterials + mesh->nPublicMaterials; index++)
    {                                                       //for all materials
//        if (mesh->localMaterial[index].texture != TR_Invalid)//if material has a texture
        if (mesh->localMaterial[index].bTexturesRegistered)
        {
            for (j = 0; j < MAX_MULTIPLAYER_PLAYERS; j++)
            {                                               //for each player
                if (((trhandle *)mesh->localMaterial[index].texture)[j] != TR_Invalid)
                {                                           //if this player was this race
#ifdef HW_PTR_64
                    if (((trhandle *)mesh->localMaterial[index].texture)[j] != 0xffffffffffffffff) // Check for -1
                    {
                        if (((trhandle *)mesh->localMaterial[index].texture)[j] < TR_RegistrySize ) {
#endif
                
                    trTextureUnregister(((trhandle *)mesh->localMaterial[index].texture)[j]);

#ifdef HW_PTR_64
                        }
                    }
#endif
                }                                           //unregister the texture
            }
            memFree((ubyte *)mesh->localMaterial[index].texture);    //free the texture handle list
        }
    }
#if MESH_RETAIN_FILENAMES
    memFree(mesh->fileName);
#endif
    memFree((void *)mesh);
}

/*-----------------------------------------------------------------------------
    Name        : meshCurrentMaterialDefault
    Description : Makes specified material current
    Inputs      : material - material to make current
                  iColorScheme - index of player associated with this mesh
                    (for texture coloring) or 0 for no player.
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void meshCurrentMaterialDefault(materialentry *material, sdword iColorScheme)
{
    GLfloat attribs[4];
    GLenum face;

    // CAT: the exe's body (meshCurrentMaterialDefault 0x004af790): fade channel 1 starts clear for every material;
    //      a dissolving ship and the build-in clip plane draw both faces; the ambient takes the render state's tints
    //      (a Beast ship red only in single player, an attribute-0x200 asteroid blue only, the Naggarok EMP
    //      flicker); no specular material; a flat material still gets GL_SMOOTH with the per-vertex colour option
    //      on (enableCPV 0x00c52378); MESH_TEAM_COLORS is off in the exe
    meshSetFade(1, 0.0f);
    if (!bitTest(material->flags, MDF_2Sided) && !catRndDissolve && !(catRndFadeFlag && catRndFadeMode == CAT_RND_FADE_CLIP))
    {
        face = GL_FRONT;
        rndBackFaceCullEnable(TRUE);                        //enable culling
        if (!usingShader) glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);   //1-sided lightmodel
    }
    else
    {                                                       //if 2-sided material
        face = GL_FRONT_AND_BACK;
        rndBackFaceCullEnable(FALSE);                       //disable culling
        if (!usingShader) glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);    //2-sided lightmodel
    }

    gSelfIllum = FALSE;
    if (g_ReplaceHack)
    {
        rndTextureEnvironment(RTE_Replace);
    }
    else if (bitTest(material->flags, MDF_SelfIllum))
    {
        gSelfIllum = TRUE;
        if (bFade)
        {
            rndTextureEnvironment(RTE_Modulate);
        }
        else
        {
            rndTextureEnvironment(RTE_Replace);
        }
    }
    else
    {
        rndTextureEnvironment(RTE_Modulate);
    }

    if (catRndRaceTint && singlePlayerGame)
    {
        attribs[0] = (GLfloat)(colRed(material->ambient)) * (1.0f / 255.0f);
        attribs[1] = 0.0f;
        attribs[2] = 0.0f;
    }
    else if (catRndAsteroidTint)
    {
        attribs[0] = 0.0f;
        attribs[1] = 0.0f;
        attribs[2] = (GLfloat)(colBlue(material->ambient)) * (1.0f / 255.0f);
    }
    else if (catRndEMPFlicker)
    {
        cat_meshEMPFlickerAmbient(attribs);
    }
    else
    {
        attribs[0] = (GLfloat)(colRed(material->ambient)) * (1.0f / 255.0f);
        attribs[1] = (GLfloat)(colGreen(material->ambient)) * (1.0f / 255.0f);
        attribs[2] = (GLfloat)(colBlue(material->ambient)) * (1.0f / 255.0f);
    }
    attribs[3] = 1.0f;
    if (usingShader)
    {
        shSetMaterial(attribs, NULL); //shader
    }
    else
    {
        glMaterialfv(face, GL_AMBIENT, attribs);
    }

    attribs[0] = (GLfloat)(colRed(material->diffuse)) * (1.0f / 255.0f);
    attribs[1] = (GLfloat)(colGreen(material->diffuse)) * (1.0f / 255.0f);
    attribs[2] = (GLfloat)(colBlue(material->diffuse)) * (1.0f / 255.0f);
    attribs[3] = bFade ? meshFadeAlpha : 1.0f;
    if (usingShader)
    {
        shSetMaterial(NULL, attribs); //shader
    }
    else
    {
        glMaterialfv(face, GL_DIFFUSE, attribs);
    }

    cat_meshMaterialTextureAndShade(material, iColorScheme);

    if (usingShader)
    {
        shUpdateLighting(); //shader
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_meshMaterialTextureAndShade
    Description : CAT: the texture and shade model part of meshCurrentMaterialDefault 0x004af790 and
                  cat_meshCurrentMaterialAlt 0x004afcb0
----------------------------------------------------------------------------*/
static void cat_meshMaterialTextureAndShade(materialentry *material, sdword iColorScheme)
{
    bool32 smooth;

    if (material->bTexturesRegistered && !g_WireframeHack)
    {                                                       //if a textured polygon
        rndTextureEnable(TRUE);
        trMakeCurrent(((trhandle *)material->texture)[iColorScheme]);
        smooth = (bitTest(material->flags, MPM_Smooth) && enableSmoothing == TRUE);
        meshPolyMode = smooth ? MPM_SmoothTexture : MPM_Texture;
    }
    else
    {
        rndTextureEnable(FALSE);
        smooth = (bitTest(material->flags, MPM_Smooth) && enableSmoothing == TRUE);
        meshPolyMode = smooth ? MPM_Smooth : MPM_Flat;
    }
    if (smooth || enableCPV)
    {
        if (!g_SpecHack)
        {
            glShadeModel(GL_SMOOTH);
        }
    }
    else
    {
        glShadeModel(GL_FLAT);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_meshEMPFlickerAmbient
    Description : CAT: the Naggarok EMP flicker of meshCurrentMaterialDefault 0x004af790 (0x004af8b2): the red and
                  green ambient swing between 0.45 and 1.0 with the physics time; each material of the frame steps
                  a copy by 0.1
----------------------------------------------------------------------------*/
static real32 catMeshEMPBase[2] = {0.0f, 0.0f};             // 0x00a4b9b8 / 0x00a4b9bc
static bool32 catMeshEMPRising = FALSE;                     // 0x00a4b9c4
static real32 catMeshEMPWork[2] = {0.0f, 0.0f};             // 0x00a4b9c8 / 0x00a4b9cc
static bool32 catMeshEMPWorkRising = FALSE;                 // 0x00a4b9d4
static void cat_meshEMPFlickerAmbient(GLfloat *attribs)
{
    if (catRndEMPFlicker != 2)
    {
        catRndEMPFlicker = 2;
        if (!catMeshEMPRising)
        {
            catMeshEMPBase[0] -= universe.phystimeelapsed;
            catMeshEMPBase[1] -= universe.phystimeelapsed;
            if (catMeshEMPBase[0] < 0.45f)                  // 0x0072af50
            {
                catMeshEMPRising = TRUE;
                catMeshEMPBase[0] = 0.45f;
                catMeshEMPBase[1] = 0.45f;
            }
        }
        else
        {
            catMeshEMPBase[0] += universe.phystimeelapsed;
            catMeshEMPBase[1] += universe.phystimeelapsed;
            if (catMeshEMPBase[0] > 1.0f)
            {
                catMeshEMPRising = FALSE;
                catMeshEMPBase[0] = 1.0f;
                catMeshEMPBase[1] = 1.0f;
            }
        }
        catMeshEMPWork[0] = catMeshEMPBase[0];
        catMeshEMPWork[1] = catMeshEMPBase[1];
        catMeshEMPWorkRising = catMeshEMPRising;
    }
    if (!catMeshEMPWorkRising)
    {
        catMeshEMPWork[0] -= 0.1f;
        catMeshEMPWork[1] -= 0.1f;
        if (catMeshEMPWork[0] < 0.45f)
        {
            catMeshEMPWorkRising = TRUE;
            catMeshEMPWork[0] = 0.45f;
            catMeshEMPWork[1] = 0.45f;
        }
    }
    else
    {
        catMeshEMPWork[0] += 0.1f;
        catMeshEMPWork[1] += 0.1f;
        if (catMeshEMPWork[0] > 1.0f)
        {
            catMeshEMPWorkRising = FALSE;
            catMeshEMPWork[0] = 1.0f;
            catMeshEMPWork[1] = 1.0f;
        }
    }
    attribs[0] = catMeshEMPWork[0];
    attribs[1] = catMeshEMPWork[1];
    attribs[2] = 1.0f;
}

/*-----------------------------------------------------------------------------
    Name        : cat_meshCurrentMaterialAlt (Cataclysm 0x004afcb0)
    Description : CAT: the material of the build-in fade's flipped pass: both faces, a dark blue ambient, the
                  material's diffuse; on a non-SW device fade channel 1 at 0.75
----------------------------------------------------------------------------*/
void cat_meshCurrentMaterialAlt(materialentry *material, sdword iColorScheme)
{
    GLfloat attribs[4];

    if (CAT_MESH_RGLTYPE != 0)
    {
        meshSetFade(1, 0.75f);                              // the channel holds 0.25 (0x3e800000)
    }
    rndBackFaceCullEnable(FALSE);
    if (!usingShader) glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    gSelfIllum = FALSE;
    if (g_ReplaceHack)
    {
        rndTextureEnvironment(RTE_Replace);
    }
    else if (bitTest(material->flags, MDF_SelfIllum) && !bFade)
    {
        rndTextureEnvironment(RTE_Replace);
    }
    else
    {
        rndTextureEnvironment(RTE_Modulate);
    }
    attribs[0] = 0.0f;
    attribs[1] = 0.0f;
    attribs[2] = 0.75f;
    attribs[3] = 1.0f;
    if (usingShader)
    {
        shSetMaterial(attribs, NULL);
    }
    else
    {
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, attribs);
    }
    attribs[0] = (GLfloat)(colRed(material->diffuse)) * (1.0f / 255.0f);
    attribs[1] = (GLfloat)(colGreen(material->diffuse)) * (1.0f / 255.0f);
    attribs[2] = (GLfloat)(colBlue(material->diffuse)) * (1.0f / 255.0f);
    attribs[3] = bFade ? meshFadeAlpha : 1.0f;
    if (usingShader)
    {
        shSetMaterial(NULL, attribs);
    }
    else
    {
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, attribs);
    }
    cat_meshMaterialTextureAndShade(material, iColorScheme);
    if (usingShader)
    {
        shUpdateLighting();
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_meshMaterialSwitch
    Description : CAT: the material switch of every mesh object renderer (meshObjectRender 0x004b0170 and its
                  siblings): the flipped pass of the build-in fade takes cat_meshCurrentMaterialAlt
----------------------------------------------------------------------------*/
static void cat_meshMaterialSwitch(materialentry *material, sdword iColorScheme)
{
    if (catRndFadeFlag && catRndFadeMode == CAT_RND_FADE_CLIP && catRndFadeFlip)
    {
        cat_meshCurrentMaterialAlt(material, iColorScheme);
    }
    else
    {
        meshCurrentMaterial(material, iColorScheme);
    }
}

/*-----------------------------------------------------------------------------
    Name        : meshCurrentMaterialTex
    Description : Makes specified material current using a single texture, not
                    the one in the material entries.
    Inputs      : material - material to make current
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void meshCurrentMaterialTex(materialentry *material, sdword iColorScheme)
{
    GLenum face;

    if (bitTest(material->flags, MDF_2Sided))
    {                                                       //if 2-sided material
        face = GL_FRONT_AND_BACK;
        rndBackFaceCullEnable(FALSE);                       //disable culling
        if (!usingShader) glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);    //2-sided lightmodel
    }
    else
    {
        face = GL_FRONT_AND_BACK;
        rndBackFaceCullEnable(TRUE);                        //enable culling
        if (!usingShader) glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);   //1-sided lightmodel
    }

    if (bitTest(material->flags, MPM_Smooth) && enableSmoothing == TRUE)
    {
        meshPolyMode = MPM_SmoothTexture;
        if (!g_SpecHack)
        {
            glShadeModel(GL_SMOOTH);
        }
    }
    else
    {
        meshPolyMode = MPM_Texture;
        glShadeModel(GL_FLAT);
    }
}

/*-----------------------------------------------------------------------------
    Name        : meshAlmostCurrentMaterial
    Description : Current material switch function that doesn't change any GL
                    interal stuff.
    Inputs      : material - the material to use
                  iColorScheme - ignored (for compatability)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void meshAlmostCurrentMaterial(materialentry* material, sdword iColorScheme)
{
    // CAT (M2-R5 render): fade channel 1 starts clear for every material, as in meshCurrentMaterialDefault
    //      (meshAlmostCurrentMaterial 0x004b0020: meshSetFade(1, 0) inlined)
    meshSetFade(1, 0.0f);
//    if (material->texture != TR_Invalid)
    if (material->bTexturesRegistered)
    {
        if (bitTest(material->flags, MPM_Smooth) && enableSmoothing == TRUE)
        {
            meshPolyMode = MPM_SmoothTexture;
        }
        else
        {
            meshPolyMode = MPM_Texture;
        }
    }
    else
    {
        if (bitTest(material->flags, MPM_Smooth) && enableSmoothing == TRUE)
        {
            meshPolyMode = MPM_Smooth;
        }
        else
        {
            meshPolyMode = MPM_Flat;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : meshSpecColour
    Description : set specular colour given parameters
    Inputs      : ...
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: two new arguments (meshSpecColour 0x004b00c0), passed on to shSpecularColour: the vertex's runtime state and
//      info entry (NULL, NULL: no glow)
void meshSpecColour(normalentry* normal, vertexentry* vertex, real32* m, real32* minv, real32* vertexState,
                    udword* vertexInfo)
{
    vector vert;
    vector norm;
    ubyte  colour[4];

    norm.x = normal->x;
    norm.y = normal->y;
    norm.z = normal->z;
    vert.x = vertex->x;
    vert.y = vertex->y;
    vert.z = vertex->z;
    colour[0] = specColour[0];
    colour[1] = specColour[1];
    colour[2] = specColour[2];
    colour[3] = specColour[3];
    shSpecularColour(specIndex, 0, &vert, &norm, colour, m, minv, vertexState, vertexInfo);
    glColor4ub(colour[0], colour[1], colour[2], colour[3]);
}

void meshMorphedSpecColour(vector* normal, real32* m, real32* minv)
{
    ubyte  colour[4];

    colour[0] = specColour[0];
    colour[1] = specColour[1];
    colour[2] = specColour[2];
    colour[3] = specColour[3];
    shSpecularColour(specIndex, 0, NULL, normal, colour, m, minv, NULL, NULL);   // CAT: no glow (meshMorphedObjectRender 0x004b34f0)
    glColor4ub(colour[0], colour[1], colour[2], colour[3]);
}

/*-----------------------------------------------------------------------------
    Name        : meshObjectRender
    Description : Render a single polygon object (part of a meshdata structure)
    Inputs      : object - object to render
                  materials - material list to use in rendering
                  iColorScheme - index of player associated with this mesh
                    (for texture coloring) or 0 for no player.
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
#if MESH_MATERIAL_STATS
    static sdword nMaterialChanges = 0;
    static sdword iMaterialMax = 0;
#endif //MESH_MATERIAL_STATS
void meshObjectOutput(polygonobject* object, materialentry* materials, sdword iColorScheme)
{
    sdword iPoly;
    polyentry* polygon;
    char *fileNameFull;
    FILE* out;

    fileNameFull = filePathPrepend("uv.txt", FF_UserSettingsPath);

    if (!fileMakeDestinationDirectory(fileNameFull))
        return;

    out = fopen(fileNameFull, "at");
    if (out == NULL)
    {
        return;
    }

    fprintf(out, "-----\n");
    fprintf(out, "%d\n", object->nPolygons);

    polygon = object->pPolygonList;                         //get first polygon list entry

    for (iPoly = 0; iPoly < object->nPolygons; iPoly++, polygon++)
    {
        fprintf(out, "%d [%0.20f %0.20f] [%0.20f %0.20f] [%0.20f %0.20f]\n",
                iPoly,
                polygon->s0, polygon->t0,
                polygon->s1, polygon->t1,
                polygon->s2, polygon->t2);
    }

    fclose(out);
}
void meshObjectPoints(polygonobject* object, materialentry* materials, sdword iColorScheme)
{
    sdword iPoly;
    vertexentry* vertexList;
    polyentry* polygon;
    bool32 lightOn, texOn;

    vertexList = object->pVertexList;
    polygon = object->pPolygonList;

    lightOn = rndLightingEnable(FALSE);
    texOn = rndTextureEnable(FALSE);

    glPointSize(4.0f);

    for (iPoly = 0; iPoly < object->nPolygons; iPoly++, polygon++)
    {
        if (iPoly == visiblePoly)
        {
            glBegin(GL_POINTS);
            glColor3ub(255,0,0);
            glVertex3fv((GLfloat*)&vertexList[polygon->iV0].x);
            glColor3ub(0,255,0);
            glVertex3fv((GLfloat*)&vertexList[polygon->iV1].x);
            glColor3ub(0,0,255);
            glVertex3fv((GLfloat*)&vertexList[polygon->iV2].x);
            glEnd();
        }
    }

    glPointSize(1.0f);

    rndLightingEnable(lightOn);
    rndTextureEnable(texOn);
}
void meshObjectRender(polygonobject *object, materialentry *materials, sdword iColorScheme)
{
    sdword iPoly;
    vertexentry *vertexList;
    polyentry *polygon;
    normalentry *normal, *normalList;
    sdword currentMaterial = -1;
    GLenum mode = GL_SMOOTH;
    sdword lightOn;
    bool32 enableBlend;

    glShadeModel(mode);

    enableBlend = bFade;

    if (g_WireframeHack)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        lightOn = rndLightingEnable(FALSE);
        glColor3ub(200,200,200);
        enableBlend = TRUE;
        if (enableBlend)
        {
            glEnable(GL_LINE_SMOOTH);
            glEnable(GL_BLEND);
            rndAdditiveBlends(FALSE);
        }
        if (!g_HiddenRemoval)
        {
            glDisable(GL_CULL_FACE);
        }
    }

    alodIncPolys(object->nPolygons);

    vertexList = object->pVertexList;                       //get base of vertex list
    normalList = object->pNormalList;                       //get base of normal list
    polygon = object->pPolygonList;                         //get first polygon list entry

    glBegin(GL_TRIANGLES);                                  //prepare to draw triangles

    for (iPoly = 0; iPoly < object->nPolygons; iPoly++)
    {
#if MESH_ANAL_CHECKING                                      //validate vertex indices
        dbgAssertOrIgnore(polygon->iV0 < object->nVertices);
        dbgAssertOrIgnore(polygon->iV1 < object->nVertices);
        dbgAssertOrIgnore(polygon->iV2 < object->nVertices);
#endif
        if (polygon->iMaterial != currentMaterial)
        {                                                   //if a new material
            glEnd();                                        //end current polygon run
            currentMaterial = polygon->iMaterial;           //remember current material
            cat_meshMaterialSwitch(&materials[currentMaterial], iColorScheme);//set new material (CAT: or the fade's alternative)
            if (enableBlend)
            {
                glEnable(GL_BLEND);
            }
            glBegin(GL_TRIANGLES);                          //start new run
#if MESH_MATERIAL_STATS
            nMaterialChanges++;                             //record material stats
            iMaterialMax = max(currentMaterial, iMaterialMax);
#endif //MESH_MATERIAL_STATS
        }

        if (g_SpecificPoly)
        {
            if  (iPoly != visiblePoly)
            {
                polygon++;
                continue;
            }
            else
            {
                real32 s, t;

                visibleUV[0][0] = polygon->s0;
                visibleUV[0][1] = polygon->t0;
                visibleUV[1][0] = polygon->s1;
                visibleUV[1][1] = polygon->t1;
                visibleUV[2][0] = polygon->s2;
                visibleUV[2][1] = polygon->t2;

                s = 0.5f / 256.0f;//(real32)scaledWidth;
                t = 0.5f / 256.0f;//(real32)scaledHeight;
                s *= (real32)visibleDirection;
                t *= (real32)visibleDirection;

                if (visibleDirection != 0)
                {
                    switch (visibleSegment)
                    {
                    case 0:
                        if (visibleWhich == 0)
                        {
                            polygon->s0 += s;
                        }
                        else
                        {
                            polygon->t0 += t;
                        }
                        break;
                    case 1:
                        if (visibleWhich == 0)
                        {
                            polygon->s1 += s;
                        }
                        else
                        {
                            polygon->t1 += t;
                        }
                        break;
                    default:
                        if (visibleWhich == 0)
                        {
                            polygon->s2 += s;
                        }
                        else
                        {
                            polygon->t2 += t;
                        }
                    }
                    visibleDirection = 0;
                }
            }
        }

        switch (meshPolyMode)
        {
            case MPM_Flat:
                dbgAssertOrIgnore(polygon->iFaceNormal != UWORD_Max);
                normal = &normalList[polygon->iFaceNormal];

                glNormal3f(normal->x, normal->y, normal->z);
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV0) }    // CAT: per-vertex colour
                glVertex3fv((GLfloat*)&vertexList[polygon->iV0].x);

                glNormal3f(normal->x, normal->y, normal->z);
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV1) }    // CAT
                glVertex3fv((GLfloat*)&vertexList[polygon->iV1].x);

                glNormal3f(normal->x, normal->y, normal->z);
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV2) }    // CAT
                glVertex3fv((GLfloat*)&vertexList[polygon->iV2].x);

#if RND_POLY_STATS
                rndNumberPolys++;
#endif //RND_POLY_STATS
                break;
            case MPM_Texture:
                dbgAssertOrIgnore(polygon->iFaceNormal != UWORD_Max);
                normal = &normalList[polygon->iFaceNormal];
                glNormal3f(normal->x, normal->y, normal->z);
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV0) }    // CAT: per-vertex colour
                glTexCoord2f(polygon->s0, polygon->t0);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV0].x);

                glNormal3f(normal->x, normal->y, normal->z);
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV1) }    // CAT
                glTexCoord2f(polygon->s1, polygon->t1);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV1].x);

                glNormal3f(normal->x, normal->y, normal->z);
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV2) }    // CAT
                glTexCoord2f(polygon->s2, polygon->t2);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV2].x);

#if RND_POLY_STATS
                rndNumberPolys++;
                rndNumberTextured++;
#endif //RND_POLY_STATS
                break;
            case MPM_Smooth:
                dbgAssertOrIgnore(vertexList[polygon->iV0].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV0].iVertexNormal];
                glNormal3f(normal->x, normal->y, normal->z);
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV0) }    // CAT: per-vertex colour
                glVertex3fv((GLfloat*)&vertexList[polygon->iV0].x);

                dbgAssertOrIgnore(vertexList[polygon->iV1].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV1].iVertexNormal];
                glNormal3f(normal->x, normal->y, normal->z);
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV1) }    // CAT
                glVertex3fv((GLfloat*)&vertexList[polygon->iV1].x);

                dbgAssertOrIgnore(vertexList[polygon->iV2].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV2].iVertexNormal];
                glNormal3f(normal->x, normal->y, normal->z);
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV2) }    // CAT
                glVertex3fv((GLfloat*)&vertexList[polygon->iV2].x);

#if RND_POLY_STATS
                rndNumberPolys++;
                rndNumberSmoothed++;
#endif //RND_POLY_STATS
                break;
            case MPM_SmoothTexture:
                dbgAssertOrIgnore(vertexList[polygon->iV0].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV0].iVertexNormal];
                glNormal3f(normal->x, normal->y, normal->z);
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV0) }    // CAT: per-vertex colour
                glTexCoord2f(polygon->s0, polygon->t0);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV0].x);

                dbgAssertOrIgnore(vertexList[polygon->iV1].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV1].iVertexNormal];
                glNormal3f(normal->x, normal->y, normal->z);
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV1) }    // CAT
                glTexCoord2f(polygon->s1, polygon->t1);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV1].x);

                dbgAssertOrIgnore(vertexList[polygon->iV2].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV2].iVertexNormal];
                glNormal3f(normal->x, normal->y, normal->z);
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV2) }    // CAT
                glTexCoord2f(polygon->s2, polygon->t2);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV2].x);

#if MESH_SURFACE_NAME_DEBUG
                if (gTestTexture)
                {
                    dbgMessagef("(%4.2f, %4.2f, %4.2f) => (%4.2f, %4.2f)", vertexList[polygon->iV0].x, vertexList[polygon->iV0].y, vertexList[polygon->iV0].z, polygon->s0, polygon->t0);
                    dbgMessagef("(%4.2f, %4.2f, %4.2f) => (%4.2f, %4.2f)", vertexList[polygon->iV1].x, vertexList[polygon->iV1].y, vertexList[polygon->iV1].z, polygon->s1, polygon->t1);
                    dbgMessagef("(%4.2f, %4.2f, %4.2f) => (%4.2f, %4.2f)", vertexList[polygon->iV2].x, vertexList[polygon->iV2].y, vertexList[polygon->iV2].z, polygon->s2, polygon->t2);
                }
#endif
#if RND_POLY_STATS
                rndNumberPolys++;
                rndNumberTextured++;
                rndNumberSmoothed++;
#endif //RND_POLY_STATS
                break;
            default:
#if MESH_ERROR_CHECKING
                dbgFatalf(DBG_Loc, "meshRender: invalid meshPolyMode: 0x%x", meshPolyMode);
#endif
                break;
        }
        polygon++;
    }
    glEnd();                                            //done drawing these triangles

    glShadeModel(GL_SMOOTH);
    if (!usingShader) glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

    if (enableBlend)
    {
        glDisable(GL_BLEND);
    }

    if (g_WireframeHack)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        rndLightingEnable(lightOn);
        if (enableBlend)
        {
            glDisable(GL_LINE_SMOOTH);
        }
    }
}


void meshEnableVertexArrays(void* vlist, sdword first, sdword count)
{
    glVertexPointer(3, GL_FLOAT, 4*sizeof(GLfloat), vlist);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_EDGE_FLAG_ARRAY);
    glDisableClientState(GL_INDEX_ARRAY);

    /*
    if (1)//glCapFeatureExists(GL_COMPILED_ARRAYS_EXT))
    {
        //glLockArraysEXT(first, count);
        glLockArrays(first, count);
    }
    */
}

void meshDisableVertexArrays(void)
{
    glDisableClientState(GL_VERTEX_ARRAY);
    /*
    if (1)//glCapFeatureExists(GL_COMPILED_ARRAYS_EXT))
    {
        //glUnlockArraysEXT();
        glUnlockArrays();
    }
    */
}

/*-----------------------------------------------------------------------------
    Name        : meshObjectRenderLit
    Description : this mesh renderer performs lighting operations itself as a
                  workaround for buggy or slow GL impl's.
                  uses ArrayElement for vertices, & will compiled arrays
    Inputs      : ...
    Outputs     :
    Return      :
    Assert      : lighting is enabled
----------------------------------------------------------------------------*/
void meshObjectRenderLit(polygonobject *object, materialentry *materials, sdword iColorScheme)
{
#ifdef __EMSCRIPTEN__
    meshObjectRender(object, materials, iColorScheme);
#else
    sdword iPoly;
    vertexentry *vertexList;
    polyentry *polygon;
    normalentry *normal, *normalList;
    sdword currentMaterial = -1;
    GLenum mode = GL_SMOOTH;

    glShadeModel(mode);

    rndLightingEnable(FALSE);

    vertexList = object->pVertexList;                       //get base of vertex list

    normalList = object->pNormalList;                       //get base of normal list
    polygon = object->pPolygonList;                         //get first polygon list entry

    alodIncPolys(object->nPolygons);

    meshEnableVertexArrays((void*)vertexList, 0, object->nVertices);

    glBegin(GL_TRIANGLES);                                  //prepare to draw triangles

    for (iPoly = 0; iPoly < object->nPolygons; iPoly++)
    {
#if MESH_ANAL_CHECKING                                      //validate vertex indices
        dbgAssertOrIgnore(polygon->iV0 < object->nVertices);
        dbgAssertOrIgnore(polygon->iV1 < object->nVertices);
        dbgAssertOrIgnore(polygon->iV2 < object->nVertices);
#endif
        if (polygon->iMaterial != currentMaterial)
        {                                                   //if a new material
            glEnd();                                        //end current polygon run
            currentMaterial = polygon->iMaterial;           //remember current material
            cat_meshMaterialSwitch(&materials[currentMaterial], iColorScheme);//set new material (CAT: or the fade's alternative)
            if (bFade)
            {
                glEnable(GL_BLEND);
            }
            glBegin(GL_TRIANGLES);                          //start new run
#if MESH_MATERIAL_STATS
            nMaterialChanges++;                             //record material stats
            iMaterialMax = max(currentMaterial, iMaterialMax);
#endif //MESH_MATERIAL_STATS
        }

        switch (meshPolyMode)
        {
            case MPM_Flat:
                dbgAssertOrIgnore(polygon->iFaceNormal != UWORD_Max);
                normal = &normalList[polygon->iFaceNormal];
                // CAT: with enableCPV every vertex gets its own colour (meshObjectRenderLit 0x004b0970)
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV0) } else { meshColour(normal) } //shader

                glArrayElement(polygon->iV0);
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV1) }
                glArrayElement(polygon->iV1);
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV2) }
                glArrayElement(polygon->iV2);
#if RND_POLY_STATS
                rndNumberPolys++;
#endif //RND_POLY_STATS
                break;
            case MPM_Texture:
                dbgAssertOrIgnore(polygon->iFaceNormal != UWORD_Max);
                normal = &normalList[polygon->iFaceNormal];
                // CAT: with enableCPV every vertex gets its own colour (meshObjectRenderLit 0x004b0970)
                if (enableCPV) { meshColourCPV(normal, object, polygon->iV0) } else { meshColour(normal) } //shader

                glTexCoord2f(polygon->s0, polygon->t0);
                glArrayElement(polygon->iV0);

                if (enableCPV) { meshColourCPV(normal, object, polygon->iV1) }
                glTexCoord2f(polygon->s1, polygon->t1);
                glArrayElement(polygon->iV1);

                if (enableCPV) { meshColourCPV(normal, object, polygon->iV2) }
                glTexCoord2f(polygon->s2, polygon->t2);
                glArrayElement(polygon->iV2);
#if RND_POLY_STATS
                rndNumberPolys++;
                rndNumberTextured++;
#endif //RND_POLY_STATS
                break;
            case MPM_Smooth:
                dbgAssertOrIgnore(vertexList[polygon->iV0].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV0].iVertexNormal];
                meshColourLit(normal, object, polygon->iV0); //shader; CAT: + info entry when enableCPV (meshObjectRenderLit 0x004b0970)
                glArrayElement(polygon->iV0);

                dbgAssertOrIgnore(vertexList[polygon->iV1].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV1].iVertexNormal];
                meshColourLit(normal, object, polygon->iV1); //shader; CAT: + info entry when enableCPV (meshObjectRenderLit 0x004b0970)
                glArrayElement(polygon->iV1);

                dbgAssertOrIgnore(vertexList[polygon->iV2].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV2].iVertexNormal];
                meshColourLit(normal, object, polygon->iV2); //shader; CAT: + info entry when enableCPV (meshObjectRenderLit 0x004b0970)
                glArrayElement(polygon->iV2);
#if RND_POLY_STATS
                rndNumberPolys++;
                rndNumberSmoothed++;
#endif //RND_POLY_STATS
                break;
            case MPM_SmoothTexture:
                dbgAssertOrIgnore(vertexList[polygon->iV0].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV0].iVertexNormal];
                meshColourLit(normal, object, polygon->iV0); //shader; CAT: + info entry when enableCPV (meshObjectRenderLit 0x004b0970)
                glTexCoord2f(polygon->s0, polygon->t0);
                glArrayElement(polygon->iV0);

                dbgAssertOrIgnore(vertexList[polygon->iV1].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV1].iVertexNormal];
                meshColourLit(normal, object, polygon->iV1); //shader; CAT: + info entry when enableCPV (meshObjectRenderLit 0x004b0970)
                glTexCoord2f(polygon->s1, polygon->t1);
                glArrayElement(polygon->iV1);

                dbgAssertOrIgnore(vertexList[polygon->iV2].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV2].iVertexNormal];
                meshColourLit(normal, object, polygon->iV2); //shader; CAT: + info entry when enableCPV (meshObjectRenderLit 0x004b0970)
                glTexCoord2f(polygon->s2, polygon->t2);
                glArrayElement(polygon->iV2);
#if MESH_SURFACE_NAME_DEBUG
                if (gTestTexture)
                {
                    dbgMessagef("\n(%4.2f, %4.2f, %4.2f) => (%4.2f, %4.2f)", vertexList[polygon->iV0].x, vertexList[polygon->iV0].y, vertexList[polygon->iV0].z, polygon->s0, polygon->t0);
                    dbgMessagef("\n(%4.2f, %4.2f, %4.2f) => (%4.2f, %4.2f)", vertexList[polygon->iV1].x, vertexList[polygon->iV1].y, vertexList[polygon->iV1].z, polygon->s1, polygon->t1);
                    dbgMessagef("\n(%4.2f, %4.2f, %4.2f) => (%4.2f, %4.2f)", vertexList[polygon->iV2].x, vertexList[polygon->iV2].y, vertexList[polygon->iV2].z, polygon->s2, polygon->t2);
                }
#endif
#if RND_POLY_STATS
                rndNumberPolys++;
                rndNumberTextured++;
                rndNumberSmoothed++;
#endif //RND_POLY_STATS
                break;
            default:
#if MESH_ERROR_CHECKING
                dbgFatalf(DBG_Loc, "meshRender: invalid meshPolyMode: 0x%x", meshPolyMode);
#endif
                break;
        }
        polygon++;
    }
    glEnd();                                            //done drawing these triangles

    meshDisableVertexArrays();

    glShadeModel(GL_SMOOTH);
    if (!usingShader) glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

    if (bFade)
    {
        glDisable(GL_BLEND);
    }

    rndLightingEnable(TRUE); //shader
#endif
}

/*-----------------------------------------------------------------------------
    Name        : meshSpecObjectRender
    Description : renders a mesh with specular-only shading.  to be used with renderers
                  that don't support specular modes in their internal pipeline (ie. !rGL)
    Inputs      : ..
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void meshSpecObjectRender(polygonobject *object, materialentry *materials, sdword iColorScheme)
{
    sdword iPoly;
    vertexentry *vertexList;
    polyentry *polygon;
    normalentry *normal, *normalList;
    sdword currentMaterial = -1;
    GLenum mode = GL_SMOOTH;
    sdword lightOn;

    real32 modelview[16], modelviewInv[16];

    lightOn = rndLightingEnable(FALSE);

    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    shInvertMatrix(modelviewInv, modelview);

    glShadeModel(mode);

    alodIncPolys(object->nPolygons);

    vertexList = object->pVertexList;                       //get base of vertex list
    normalList = object->pNormalList;                       //get base of normal list
    polygon = object->pPolygonList;                         //get first polygon list entry

    glBegin(GL_TRIANGLES);                                  //prepare to draw triangles

    for (iPoly = 0; iPoly < object->nPolygons; iPoly++)
    {
#if MESH_ANAL_CHECKING                                      //validate vertex indices
        dbgAssertOrIgnore(polygon->iV0 < object->nVertices);
        dbgAssertOrIgnore(polygon->iV1 < object->nVertices);
        dbgAssertOrIgnore(polygon->iV2 < object->nVertices);
#endif
        if (polygon->iMaterial != currentMaterial)
        {                                                   //if a new material
            glEnd();                                        //end current polygon run
            currentMaterial = polygon->iMaterial;           //remember current material
            cat_meshMaterialSwitch(&materials[currentMaterial], iColorScheme);//set new material (CAT: or the fade's alternative)
            glBegin(GL_TRIANGLES);                          //start new run
#if MESH_MATERIAL_STATS
            nMaterialChanges++;                             //record material stats
            iMaterialMax = max(currentMaterial, iMaterialMax);
#endif //MESH_MATERIAL_STATS
        }

        switch (meshPolyMode)
        {
            case MPM_Flat:
                dbgAssertOrIgnore(polygon->iFaceNormal != UWORD_Max);
                normal = &normalList[polygon->iFaceNormal];

                meshSpecColourCPV(normal, vertexList, object, polygon->iV0, modelview, modelviewInv);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV0]);

                meshSpecColourCPV(normal, vertexList, object, polygon->iV1, modelview, modelviewInv);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV1]);

                meshSpecColourCPV(normal, vertexList, object, polygon->iV2, modelview, modelviewInv);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV2]);

#if RND_POLY_STATS
                rndNumberPolys++;
#endif //RND_POLY_STATS
                break;
            case MPM_Texture:
                dbgAssertOrIgnore(polygon->iFaceNormal != UWORD_Max);
                normal = &normalList[polygon->iFaceNormal];

                glTexCoord2f(polygon->s0, polygon->t0);
                meshSpecColourCPV(normal, vertexList, object, polygon->iV0, modelview, modelviewInv);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV0]);

                glTexCoord2f(polygon->s1, polygon->t1);
                meshSpecColourCPV(normal, vertexList, object, polygon->iV1, modelview, modelviewInv);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV1]);

                glTexCoord2f(polygon->s2, polygon->t2);
                meshSpecColourCPV(normal, vertexList, object, polygon->iV2, modelview, modelviewInv);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV2]);

#if RND_POLY_STATS
                rndNumberPolys++;
                rndNumberTextured++;
#endif //RND_POLY_STATS
                break;
            case MPM_Smooth:
                dbgAssertOrIgnore(vertexList[polygon->iV0].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV0].iVertexNormal];
                meshSpecColourCPV(normal, vertexList, object, polygon->iV0, modelview, modelviewInv);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV0]);

                dbgAssertOrIgnore(vertexList[polygon->iV1].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV1].iVertexNormal];
                meshSpecColourCPV(normal, vertexList, object, polygon->iV1, modelview, modelviewInv);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV1]);

                dbgAssertOrIgnore(vertexList[polygon->iV2].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV2].iVertexNormal];
                meshSpecColourCPV(normal, vertexList, object, polygon->iV2, modelview, modelviewInv);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV2]);

#if RND_POLY_STATS
                rndNumberPolys++;
                rndNumberSmoothed++;
#endif //RND_POLY_STATS
                break;
            case MPM_SmoothTexture:
                dbgAssertOrIgnore(vertexList[polygon->iV0].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV0].iVertexNormal];
                glTexCoord2f(polygon->s0, polygon->t0);
                meshSpecColourCPV(normal, vertexList, object, polygon->iV0, modelview, modelviewInv);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV0]);

                dbgAssertOrIgnore(vertexList[polygon->iV1].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV1].iVertexNormal];
                glTexCoord2f(polygon->s1, polygon->t1);
                meshSpecColourCPV(normal, vertexList, object, polygon->iV1, modelview, modelviewInv);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV1]);

                dbgAssertOrIgnore(vertexList[polygon->iV2].iVertexNormal != UWORD_Max);
                normal = &normalList[vertexList[polygon->iV2].iVertexNormal];
                glTexCoord2f(polygon->s2, polygon->t2);
                meshSpecColourCPV(normal, vertexList, object, polygon->iV2, modelview, modelviewInv);
                glVertex3fv((GLfloat*)&vertexList[polygon->iV2]);

#if RND_POLY_STATS
                rndNumberPolys++;
                rndNumberTextured++;
                rndNumberSmoothed++;
#endif //RND_POLY_STATS
                break;
            default:
#if MESH_ERROR_CHECKING
                dbgFatalf(DBG_Loc, "meshRender: invalid meshPolyMode: 0x%x", meshPolyMode);
#endif
                break;
        }
        polygon++;
    }
    glEnd();                                            //done drawing these triangles

    glShadeModel(GL_SMOOTH);
    glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
    rndLightingEnable(lightOn);
}

/*-----------------------------------------------------------------------------
    Name        : meshLerpVerts
    Description : Helper for morphed object rendering.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void meshLerpVerts(vector* c, vertexentry* a, vertexentry* b, real32 frac)
{
    real32 ofrac;

    ofrac = 1.0f - frac;

    c->x = ofrac * a->x + frac * b->x;
    c->y = ofrac * a->y + frac * b->y;
    c->z = ofrac * a->z + frac * b->z;
}

/*-----------------------------------------------------------------------------
    Name        : meshLerpNormal
    Description : Helper for morphed object rendering.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void meshLerpNormal(vector* c, normalentry* a, normalentry* b, real32 frac)
{
    real32 ofrac;

    ofrac = 1.0f - frac;

    c->x = ofrac * a->x + frac * b->x;
    c->y = ofrac * a->y + frac * b->y;
    c->z = ofrac * a->z + frac * b->z;
}

/*-----------------------------------------------------------------------------
    Name        : meshMorphedObjectRender
    Description : Render a mesh object morphed between two endpoint objects
    Inputs      : object1, object2 - objects being lerped between
                  uvPolys - polygons to get the u/v coordinates from.  Must be
                    exactly the same as the polygon objects.
                  materials - material list for this object.
                  frac - how much of object2 to use.
                  iColorScheme - color scheme to render in, if any
    Outputs     :
    Return      :
    Note        : use object1's stuff, but lerp to object2
----------------------------------------------------------------------------*/
void meshMorphedObjectRender(
    polygonobject* object1, polygonobject* object2, polyentry *uvPolys,
    materialentry* materials, real32 frac, sdword iColorScheme)
{
    sdword iPoly;
    vertexentry* vertexList1;
    vertexentry* vertexList2;
    polyentry*   polygon;
    polyentry*   uvPolygon;
    normalentry* normalList1;
    normalentry* normalList2;
    sdword currentMaterial = -1;
    vector vert0, vert1, vert2, normal;
    GLenum mode = GL_SMOOTH;
#if MESH_MORPH_DEBUG
    sdword colorIndex = 0;
    color  morphLineColor1, morphLineColor2;
    GLboolean texEnabled, lightEnabled, blendEnabled;
#endif
    bool32 lightOn = FALSE;
    real32 modelview[16], modelviewInv[16];

    dbgAssertOrIgnore(object1->nVertices == object2->nVertices);
    dbgAssertOrIgnore(object1->nPolygons == object2->nPolygons);
    glShadeModel(mode);

    if (g_SpecHack)
    {
        lightOn = rndLightingEnable(FALSE);
        glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
        shInvertMatrix(modelviewInv, modelview);
    }

    alodIncPolys(object1->nPolygons);

    vertexList1 = object1->pVertexList;
    vertexList2 = object2->pVertexList;
    normalList1 = object1->pNormalList;
    normalList2 = object2->pNormalList;
    polygon = object1->pPolygonList;
    uvPolygon = uvPolys;
    glBegin(GL_TRIANGLES);

    for (iPoly = 0; iPoly < object1->nPolygons; iPoly++)
    {
#if MESH_ANAL_CHECKING
        dbgAssertOrIgnore(polygon->iV0 < object1->nVertices);
        dbgAssertOrIgnore(polygon->iV1 < object1->nVertices);
        dbgAssertOrIgnore(polygon->iV2 < object1->nVertices);
#endif
        if (polygon->iMaterial != currentMaterial)
        {
            glEnd();
            currentMaterial = polygon->iMaterial;
            cat_meshMaterialSwitch(&materials[currentMaterial], iColorScheme);   // CAT: or the fade's alternative
            glBegin(GL_TRIANGLES);
#if MESH_MATERIAL_STATS
            nMaterialChanges++;
            iMaterialMax = max(currentMaterial, iMaterialMax);
#endif //MESH_MATERIAL_STATS
        }
        meshLerpVerts(&vert0, &vertexList1[polygon->iV0], &vertexList2[polygon->iV0], frac);
        meshLerpVerts(&vert1, &vertexList1[polygon->iV1], &vertexList2[polygon->iV1], frac);
        meshLerpVerts(&vert2, &vertexList1[polygon->iV2], &vertexList2[polygon->iV2], frac);
#if MESH_MORPH_DEBUG                                        //morphed mesh debug code
        if (meshMorphDebug)
        {
            //disable a bunch of stuff
            glEnd();
            texEnabled   = glIsEnabled(GL_TEXTURE_2D);
            lightEnabled = glIsEnabled(GL_LIGHTING);
            blendEnabled = glIsEnabled(GL_BLEND);
            glDisable(GL_TEXTURE_2D);
            glDisable(GL_LIGHTING);
            glDisable(GL_BLEND);

            //draw outlines of endpoint meshes
            morphLineColor1 = meshMorphLineColors[colorIndex].from;
            morphLineColor2 = meshMorphLineColors[colorIndex].to;
            colorIndex++;                                   //choose a color for the line
            if (meshMorphLineColors[colorIndex].from == colBlack)
            {
                colorIndex = 0;
            }
            if (!keyIsHit(ONEKEY))
            {
                primLine3((vector *)&vertexList1[polygon->iV0], (vector *)&vertexList1[polygon->iV1], morphLineColor2);
            }
            if (!keyIsHit(TWOKEY))
            {
                primLine3((vector *)&vertexList2[polygon->iV0], (vector *)&vertexList2[polygon->iV1], morphLineColor1);
            }
            morphLineColor1 = meshMorphLineColors[colorIndex].from;
            morphLineColor2 = meshMorphLineColors[colorIndex].to;
            colorIndex++;                                   //choose a color for the line
            if (meshMorphLineColors[colorIndex].from == colBlack)
            {
                colorIndex = 0;
            }
            if (!keyIsHit(ONEKEY))
            {
                primLine3((vector *)&vertexList1[polygon->iV1], (vector *)&vertexList1[polygon->iV2], morphLineColor2);
            }
            if (!keyIsHit(TWOKEY))
            {
                primLine3((vector *)&vertexList2[polygon->iV1], (vector *)&vertexList2[polygon->iV2], morphLineColor1);
            }
            morphLineColor1 = meshMorphLineColors[colorIndex].from;
            morphLineColor2 = meshMorphLineColors[colorIndex].to;
            colorIndex++;                                   //choose a color for the line
            if (meshMorphLineColors[colorIndex].from == colBlack)
            {
                colorIndex = 0;
            }
            if (!keyIsHit(ONEKEY))
            {
                primLine3((vector *)&vertexList1[polygon->iV2], (vector *)&vertexList1[polygon->iV0], morphLineColor2);
            }
            if (!keyIsHit(TWOKEY))
            {
                primLine3((vector *)&vertexList2[polygon->iV2], (vector *)&vertexList2[polygon->iV0], morphLineColor1);
            }
            //draw the connecting lines
            glEnable(GL_LINE_STIPPLE);
            glLineStipple(1, 0x5a5a);
            if (!keyIsHit(THREEKEY))
            {
                primLine3((vector *)&vertexList1[polygon->iV0], (vector *)&vertexList2[polygon->iV0], colWhite);
                primLine3((vector *)&vertexList1[polygon->iV1], (vector *)&vertexList2[polygon->iV1], colWhite);
                primLine3((vector *)&vertexList1[polygon->iV2], (vector *)&vertexList2[polygon->iV2], colWhite);
            }
            glDisable(GL_LINE_STIPPLE);
            if (!keyIsHit(FOURKEY))
            {
                primLine3(&vert0, &vert1, colRGB(128, 128, 128));
                primLine3(&vert1, &vert2, colRGB(128, 128, 128));
                primLine3(&vert2, &vert0, colRGB(128, 128, 128));
            }

            if (texEnabled)
            {
                glEnable(GL_TEXTURE_2D);
            }
            if (texEnabled)
            {
                glEnable(GL_LIGHTING);
            }
            if (blendEnabled)
            {
                glEnable(GL_BLEND);
            }
            glBegin(GL_TRIANGLES);
        }
#endif //MESH_MORPH_DEBUG
        switch (meshPolyMode)
        {
            case MPM_Flat:
                dbgAssertOrIgnore(polygon->iFaceNormal != UWORD_Max);
                if (mode != GL_FLAT)
                {
                    mode = GL_FLAT;
                    glShadeModel(mode);
                }

                meshLerpNormal(&normal, &normalList1[polygon->iFaceNormal], &normalList2[polygon->iFaceNormal], frac);
                if (g_SpecHack) meshMorphedSpecColour(&normal, modelview, modelviewInv);
                glNormal3f(normal.x, normal.y, normal.z);
                glVertex3fv((GLfloat*)&vert0);

                glNormal3f(normal.x, normal.y, normal.z);
                glVertex3fv((GLfloat*)&vert1);

                glNormal3f(normal.x, normal.y, normal.z);
                glVertex3fv((GLfloat*)&vert2);
#if RND_POLY_STATS
                rndNumberPolys++;
#endif //RND_POLY_STATS
                break;
            case MPM_Texture:
                dbgAssertOrIgnore(polygon->iFaceNormal != UWORD_Max);
                if (mode != GL_FLAT)
                {
                    mode = GL_FLAT;
                    glShadeModel(mode);
                }

                meshLerpNormal(&normal, &normalList1[polygon->iFaceNormal], &normalList2[polygon->iFaceNormal], frac);
                if (g_SpecHack) meshMorphedSpecColour(&normal, modelview, modelviewInv);
                glNormal3f(normal.x, normal.y, normal.z);
                glTexCoord2f(uvPolygon->s0, uvPolygon->t0);
                glVertex3fv((GLfloat*)&vert0);
                
                glNormal3f(normal.x, normal.y, normal.z);
                glTexCoord2f(uvPolygon->s1, uvPolygon->t1);
                glVertex3fv((GLfloat*)&vert1);
                
                glNormal3f(normal.x, normal.y, normal.z);
                glTexCoord2f(uvPolygon->s2, uvPolygon->t2);
                glVertex3fv((GLfloat*)&vert2);
#if RND_POLY_STATS
                rndNumberPolys++;
                rndNumberTextured++;
#endif //RND_POLY_STATS
                break;
            case MPM_Smooth:
                dbgAssertOrIgnore(vertexList1[polygon->iV0].iVertexNormal != UWORD_Max);
                if (mode != GL_SMOOTH)
                {
                    mode = GL_SMOOTH;
                    glShadeModel(mode);
                }

                meshLerpNormal(&normal, &normalList1[vertexList1[polygon->iV0].iVertexNormal],
                               &normalList2[vertexList2[polygon->iV0].iVertexNormal], frac);
                if (g_SpecHack) meshMorphedSpecColour(&normal, modelview, modelviewInv);
                glNormal3f(normal.x, normal.y, normal.z);
                glVertex3fv((GLfloat*)&vert0);

                dbgAssertOrIgnore(vertexList1[polygon->iV1].iVertexNormal != UWORD_Max);
                meshLerpNormal(&normal, &normalList1[vertexList1[polygon->iV1].iVertexNormal],
                               &normalList2[vertexList2[polygon->iV1].iVertexNormal], frac);
                if (g_SpecHack) meshMorphedSpecColour(&normal, modelview, modelviewInv);
                glNormal3f(normal.x, normal.y, normal.z);
                glVertex3fv((GLfloat*)&vert1);

                dbgAssertOrIgnore(vertexList1[polygon->iV2].iVertexNormal != UWORD_Max);
                meshLerpNormal(&normal, &normalList1[vertexList1[polygon->iV2].iVertexNormal],
                               &normalList2[vertexList2[polygon->iV2].iVertexNormal], frac);
                if (g_SpecHack) meshMorphedSpecColour(&normal, modelview, modelviewInv);
                glNormal3f(normal.x, normal.y, normal.z);
                glVertex3fv((GLfloat*)&vert2);
#if RND_POLY_STATS
                rndNumberPolys++;
                rndNumberSmoothed++;
#endif //RND_POLY_STATS
                break;
            case MPM_SmoothTexture:
                if (mode != GL_SMOOTH)
                {
                    mode = GL_SMOOTH;
                    glShadeModel(mode);
                }

                dbgAssertOrIgnore(vertexList1[polygon->iV0].iVertexNormal != UWORD_Max);
                meshLerpNormal(&normal, &normalList1[vertexList1[polygon->iV0].iVertexNormal],
                               &normalList2[vertexList2[polygon->iV0].iVertexNormal], frac);
                if (g_SpecHack) meshMorphedSpecColour(&normal, modelview, modelviewInv);
                glNormal3f(normal.x, normal.y, normal.z);
                glTexCoord2f(uvPolygon->s0, uvPolygon->t0);
                glVertex3fv((GLfloat*)&vert0);

                dbgAssertOrIgnore(vertexList1[polygon->iV1].iVertexNormal != UWORD_Max);
                meshLerpNormal(&normal, &normalList1[vertexList1[polygon->iV1].iVertexNormal],
                               &normalList2[vertexList2[polygon->iV1].iVertexNormal], frac);
                if (g_SpecHack) meshMorphedSpecColour(&normal, modelview, modelviewInv);
                glNormal3f(normal.x, normal.y, normal.z);
                glTexCoord2f(uvPolygon->s1, uvPolygon->t1);
                glVertex3fv((GLfloat*)&vert1);

                dbgAssertOrIgnore(vertexList1[polygon->iV2].iVertexNormal != UWORD_Max);
                meshLerpNormal(&normal, &normalList1[vertexList1[polygon->iV2].iVertexNormal],
                               &normalList2[vertexList2[polygon->iV2].iVertexNormal], frac);
                if (g_SpecHack) meshMorphedSpecColour(&normal, modelview, modelviewInv);
                glNormal3f(normal.x, normal.y, normal.z);
                glTexCoord2f(uvPolygon->s2, uvPolygon->t2);
                glVertex3fv((GLfloat*)&vert2);
#if RND_POLY_STATS
                rndNumberPolys++;
                rndNumberTextured++;
                rndNumberSmoothed++;
#endif //RND_POLY_STATS
                break;
            default:
#if MESH_ERROR_CHECKING
                dbgFatalf(DBG_Loc, "meshRender: invalid meshPolyMode: 0x%x", meshPolyMode);
#endif
                break;
        }
        polygon++;
        uvPolygon++;
    }
    glEnd();

    if (g_SpecHack)
    {
        rndLightingEnable(lightOn);
    }

    glShadeModel(GL_SMOOTH);
    glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : meshMorphedObjectRenderTex
    Description : Render a morphed mesh object, lerping between two meshes with
                    a single texture map.
    Inputs      : object1, object2 - objects being lerped between
                  uvPolys - polygons to get the u/v coordinates from.  Must be
                    exactly the same as the polygon objects.
                  materials - material list for this object.
                  frac - how much of object2 to use.
                  iColorScheme - color scheme to render in, or 0 if none.
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void meshMorphedObjectRenderTex(polygonobject* object1, polygonobject* object2,
                                polyentry *uvPolys, materialentry* material,
                                real32 frac, sdword iColorScheme)
{
//    dbgMessagef("meshCurrentMaterialTex O1: 0x%lx O2: 0x%lx", object1,object2); // Something very bad was happening here. :(
    meshCurrentMaterial = meshCurrentMaterialTex;
    meshMorphedObjectRender(object1, object2, uvPolys, material, frac, 0);
    meshCurrentMaterial = meshCurrentMaterialDefault;
}

/*-----------------------------------------------------------------------------
    Name        : meshObjectRenderTex
    Description : Renders an object using a single texture map for the entire surface
    Inputs      : object - object to render
                  material - materials to use (surface attribs will be interpreted properly)
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void meshObjectRenderTex(polygonobject *object, materialentry *material)
{
    meshCurrentMaterial = meshCurrentMaterialTex;
    meshObjectRender(object, material, 0);
    meshCurrentMaterial = meshCurrentMaterialDefault;
}

/*-----------------------------------------------------------------------------
    Name        : meshObjectRenderR
    Description : Render a single polygon object and all it's 'offspring' objects.
    Inputs      : object - object to render.
                  materials - material list to use in rendering
                  iColorScheme - index of player associated with this mesh
                    (for texture coloring) or 0 for no player.
    Outputs     : renders the object and all it's daughter objects.
    Return      : void
----------------------------------------------------------------------------*/
//CAT: meshObjectRenderR 0x004b4110 also takes the level's vertex-state slots and the object's slot index (the same
//pre-order as cat_univMeshStateAllocRecursive 0x00541b80) and returns the next index
static struct CatMeshObjectState *meshCurrentStateSlots = NULL;
static mhlocalbinding *meshCurrentBindingBase = NULL;   //CAT: first binding of the LOD being drawn (slot index base)

/*-----------------------------------------------------------------------------
    Name        : cat_meshRenderState
    Description : meshRender with a ship's per-vertex mesh state: the exe's meshRender 0x004b4280 takes the LOD's
                  slots as its first argument (cat_rndRenderObjectList passes them for ships with no local
                  bindings); every other caller passes none, which is what meshRender does here.
    Inputs      : slots - the LOD's object slots or NULL; mesh, iColorScheme - as meshRender
    Outputs     :
    Return      : as meshRender
----------------------------------------------------------------------------*/
sdword meshRender(meshdata *mesh, sdword iColorScheme);
sdword cat_meshRenderState(struct CatMeshObjectState *slots, meshdata *mesh, sdword iColorScheme)
{
    sdword result;

    meshCurrentStateSlots = slots;
    result = meshRender(mesh, iColorScheme);
    meshCurrentStateSlots = NULL;
    return result;
}
sdword meshObjectRenderR(polygonobject *object, materialentry *materials, sdword iColorScheme, sdword index)
{
    polygonobject *daughter;
    sdword next = index + 1;                                //CAT

    glPushMatrix();
    glMultMatrixf((float *)&object->localMatrix);            //set matrix for this object
	shPushLightMatrix(&object->localMatrix);

    for (daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
    {
        next = meshObjectRenderR(daughter, materials, iColorScheme, next);     //render all daughter objects
    }

    meshCurrentVertexState = MESH_SLOT_STATE(meshCurrentStateSlots, index);   //CAT
    MESH_OBJECT_RENDER(object, materials, iColorScheme)
    meshCurrentVertexState = NULL;                          //CAT

    shPopLightMatrix();
    glPopMatrix();
    return next;
}

/*-----------------------------------------------------------------------------
    Name        : meshRender
    Description : Render the specified mesh
    Inputs      : mesh - mesh to be rendered
                  iColorScheme - index of player associated with this mesh
                    (for texture coloring) or 0 for no player.
    Outputs     : ..
    Return      : ??
    Note        : This function does not even call any GL wrappers.
----------------------------------------------------------------------------*/
sdword meshRender(meshdata *mesh, sdword iColorScheme)
{
    polygonobject *object;
    sdword index;                                           //CAT

    meshFixupPacoUV(mesh, texLinearFiltering);

    if (g_NoMatSwitch)
    {
        meshCurrentMaterial = meshAlmostCurrentMaterial;
    }
    else
    {
        meshCurrentMaterial = meshCurrentMaterialDefault;
    }
#if MESH_MATERIAL_STATS
    nMaterialChanges = 0;
    iMaterialMax = 0;
#endif //MESH_MATERIAL_STATS
    for (object = &mesh->object[0], index = 0; object != NULL; object = object->pSister)
    {
        index = meshObjectRenderR(object, mesh->localMaterial, iColorScheme, index);  //CAT: slot index (0x004b4280)
    }
#if MESH_MATERIAL_STATS
    meshMaterialChanges += nMaterialChanges;                //record material stats
    meshTotalMaterials += iMaterialMax + 1;
#endif //MESH_MATERIAL_STATS
    meshCurrentMaterial = meshCurrentMaterialDefault;
    return(0);
}

/*-----------------------------------------------------------------------------
    Name        : meshObjectRenderHierarchy
    Description : Render a polygon object and all it's offspring
    Inputs      : binding - base of a binding list which is updated and returned
                  everything else - same as meshObjectRender
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
mhlocalbinding *meshObjectRenderHierarchy(mhlocalbinding *binding, polygonobject *object, materialentry *materials, sdword iColorScheme)
{
    polygonobject *daughter;
    //CAT: the exe walks the bindings by index (0x004b4370); the index picks the object's vertex-state slot
    sdword index = (meshCurrentStateSlots != NULL) ? (sdword)(binding - meshCurrentBindingBase) : 0;

    glPushMatrix();
    if (binding->function != NULL)
    {
        binding->function(binding->flags, &object->localMatrix,//call matrix update function !!! called every frame
                          &binding->matrix, binding->userData, binding->userID);
        glMultMatrixf((float *)&binding->matrix);           //set matrix for this object
		shPushLightMatrix(&binding->matrix);
    }
    else
    {
        glMultMatrixf((float *)&object->localMatrix);       //set matrix for this object
		shPushLightMatrix(&object->localMatrix);
    }
    binding++;
    for (daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
    {
        binding = meshObjectRenderHierarchy(binding, daughter, materials, iColorScheme);     //render all daughter objects
    }
    meshCurrentVertexState = MESH_SLOT_STATE(meshCurrentStateSlots, index);   //CAT: this object's vertex states
    MESH_OBJECT_RENDER(object, materials, iColorScheme)
    meshCurrentVertexState = NULL;                          //CAT
    shPopLightMatrix();
    glPopMatrix();
    return(binding);
}

/*-----------------------------------------------------------------------------
    Name        : meshRenderShipHierarchy
    Description : Render a specified LOD of a ship using it's hierarchy binding
                    info.
    Inputs      : ship - ship to render
                  currentLOD - LOD to render at.
                  mesh - mesh to render normally if a proper hierarchy cannot be found.
    Outputs     : renders a ship
    Return      : void
----------------------------------------------------------------------------*/
void meshRenderShipHierarchy(struct CatMeshState *meshState, shipbindings *bindings, sdword currentLOD, meshdata *mesh, sdword iColorScheme)
{
    polygonobject *object;
    mhlocalbinding *binding;                                //binding list for this LOD

    meshFixupPacoUV(mesh, texLinearFiltering);

#if MESH_PRE_CALLBACK
    if (bindings->preCallback)
    {                                                       //call the pre callback
        bindings->preCallback(mesh, bindings, currentLOD);
    }
#endif
    if (g_NoMatSwitch)
    {
        meshCurrentMaterial = meshAlmostCurrentMaterial;
    }
    else
    {
        meshCurrentMaterial = meshCurrentMaterialDefault;
    }
#if MESH_MATERIAL_STATS
    nMaterialChanges = 0;
    iMaterialMax = 0;
#endif //MESH_MATERIAL_STATS
    binding = bindings->localBinding[currentLOD];
    //CAT: meshRenderShipHierarchy 0x004b4520 takes the ship's per-vertex mesh state as a new first argument and
    //passes this LOD's slots down (meshState->level[currentLOD], or none)
    meshCurrentBindingBase = binding;
    meshCurrentStateSlots = (meshState != NULL) ? meshState->level[currentLOD] : NULL;
    for (object = &mesh->object[0]; object != NULL; object = object->pSister)
    {
        binding = meshObjectRenderHierarchy(binding, object, mesh->localMaterial, iColorScheme);
    }
    meshCurrentStateSlots = NULL;
    meshCurrentBindingBase = NULL;
#if MESH_MATERIAL_STATS
    meshMaterialChanges += nMaterialChanges;                //record material stats
    meshTotalMaterials += iMaterialMax + 1;
#endif //MESH_MATERIAL_STATS
    if (bindings->postCallback)
    {                                                       //call the post callback
        bindings->postCallback(mesh, bindings, currentLOD);
    }
    meshCurrentMaterial = meshCurrentMaterialDefault;
}

/*-----------------------------------------------------------------------------
    Name        : meshConcatByUserData
    Description : The rest of the recursion function of meshFindHierarchyMatrixByUserData
    Inputs      : binding - starting binding pointer.
                  object - current object in hierarchy.
                  userData - data to compare against.
    Outputs     : dest - where to store the concatenated matrix.
    Return      :
----------------------------------------------------------------------------*/
mhbinding *meshConcatByUserData(hmatrix *dest, hmatrix *destNC, mhbinding *binding, polygonobject *object, void *userData)
{
    polygonobject *daughter;
    hmatrix mat;

    hmatMultiplyHMatByHMat(&mat, dest, &object->localMatrix);//concatenate a matrix
    if (binding->userData == userData)
    {                                                       //if found user data we were looking for
        *dest = mat;
        *destNC = object->localMatrix;
        return(NULL);                                       //stop looking
    }
    binding++;
    for (daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
    {                                                       //for all child objects
        binding = meshConcatByUserData(&mat, destNC, binding,
                                       daughter, userData);//concatenate all daughter objects
        if (binding == NULL)
        {                                                   //if that tree found it
            *dest = mat;
            return(NULL);
        }
    }
    return(binding);
}

/*-----------------------------------------------------------------------------
    Name        : meshFindHierarchyMatrixByUserData
    Description : Create a concatenated matrix for the mesh hierarchy member
                    with the specified user data pointer.
    Inputs      :
                  bindings - list of mesh bindings
                  userData - userData pointer to compare against.
    Outputs     : dest - where to store the concatenated matrix.
    Return      : TRUE if found, FALSE otherwise
----------------------------------------------------------------------------*/
bool32 meshFindHierarchyMatrixByUserData(hmatrix *dest, hmatrix *destNC, mhbinding *binding, void *userData)
{
    polygonobject *object;

    *dest = IdentityHMatrix;
    for (object = &binding->object[0]; object != NULL; object = object->pSister)
    {
        dbgAssertOrIgnore(binding->object == object);
        binding = meshConcatByUserData(dest, destNC, binding, object, userData);
        if (binding == NULL)
        {
            return(TRUE);
        }
    }
    return(FALSE);
}

/*=============================================================================
    CAT: the bullet-hit glow (cat_meshApplyBulletHit 0x004b52d0 and its helpers 0x004b4f80..0x004b5240, callback
    0x004b5820). A bullet that hits a ship lights the mesh vertex nearest to the hit point and every vertex within a
    radius of it, in every LOD of the ship's per-vertex mesh state. The exe builds each object's matrix on the GL
    modelview stack (glPushMatrix / glMultMatrixf / glGetFloatv(GL_MODELVIEW_MATRIX)) from the ship's coordinate
    system and position and the objects' local matrices; this port multiplies the same matrices on the CPU (the
    glow is render state only, so the last float bits of the result do not matter to the simulation). The radius
    pass compares object-space positions with the nearest vertex's object-space position, as the exe does.
=============================================================================*/
typedef struct CatNearestVertex                         // the exe's 0x18-byte record
{
    vertexinfo *info;                                   // +0x00 the nearest vertex's info entry, NULL = none yet
    CatVertexState *state;                              // +0x04 its state
    real32 distSquared;                                 // +0x08
    vector position;                                    // +0x0C its object-space position
} CatNearestVertex;

//cat_meshObjectNearestVertexToHit 0x004b4f80: brings the hit point into object space (the inverse rotation of the
//object-to-world matrix times hit - translation) and keeps the object's vertex nearest to it in the record
static void cat_meshObjectNearestVertexToHit(vector *hit, hmatrix *objectToWorld, polygonobject *object, sdword index,
                                             CatMeshObjectState *slots, CatNearestVertex *rec)
{
    matrix rotation;
    vector delta, local, d;
    real32 distSquared;
    sdword i;

    rotation.m11 = objectToWorld->m11; rotation.m21 = objectToWorld->m21; rotation.m31 = objectToWorld->m31;
    rotation.m12 = objectToWorld->m12; rotation.m22 = objectToWorld->m22; rotation.m32 = objectToWorld->m32;
    rotation.m13 = objectToWorld->m13; rotation.m23 = objectToWorld->m23; rotation.m33 = objectToWorld->m33;
    delta.x = hit->x - objectToWorld->m14;
    delta.y = hit->y - objectToWorld->m24;
    delta.z = hit->z - objectToWorld->m34;
    matMultiplyVecByMat(&local, &delta, &rotation);
    for (i = 0; i < object->nVertices; i++)
    {
        d.x = local.x - object->pVertexList[i].x;
        d.y = local.y - object->pVertexList[i].y;
        d.z = local.z - object->pVertexList[i].z;
        distSquared = d.x * d.x + d.y * d.y + d.z * d.z;
        if (rec->info == NULL || distSquared < rec->distSquared)
        {
            rec->info = &object->pVertexInfo[i];
            rec->state = &slots[index].vertex[i];
            rec->distSquared = distSquared;
            rec->position.x = object->pVertexList[i].x;
            rec->position.y = object->pVertexList[i].y;
            rec->position.z = object->pVertexList[i].z;
        }
    }
}

//cat_meshObjectNearestVertexToHitR 0x004b5110: the daughters first (slots index + 1 ...), then the object itself
static sdword cat_meshObjectNearestVertexToHitR(vector *hit, hmatrix *parentToWorld, polygonobject *object, sdword index,
                                                CatMeshObjectState *slots, CatNearestVertex *rec)
{
    polygonobject *daughter;
    hmatrix objectToWorld;
    sdword next = index + 1;

    hmatMultiplyHMatByHMat(&objectToWorld, parentToWorld, &object->localMatrix);    // glPushMatrix, glMultMatrixf
    for (daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
    {
        next = cat_meshObjectNearestVertexToHitR(hit, &objectToWorld, daughter, next, slots, rec);
    }
    cat_meshObjectNearestVertexToHit(hit, &objectToWorld, object, index, slots, rec);
    return next;
}

//cat_meshObjectForVerticesInRadius 0x004b5190: calls the callback for every vertex of the object within the squared
//radius of the nearest vertex's (object-space) position
static void cat_meshObjectForVerticesInRadius(CatNearestVertex *rec, real32 radiusSquared, polygonobject *object,
                                              sdword index, CatMeshObjectState *slots,
                                              void (*callback)(vertexinfo *, CatVertexState *, void *), void *data)
{
    vector d;
    sdword i;

    for (i = 0; i < object->nVertices; i++)
    {
        d.x = object->pVertexList[i].x - rec->position.x;
        d.y = object->pVertexList[i].y - rec->position.y;
        d.z = object->pVertexList[i].z - rec->position.z;
        if (d.x * d.x + d.y * d.y + d.z * d.z <= radiusSquared)
        {
            callback(&object->pVertexInfo[i], &slots[index].vertex[i], data);
        }
    }
}

//cat_meshObjectForVerticesInRadiusR 0x004b5240: the daughters first, then the object
static sdword cat_meshObjectForVerticesInRadiusR(CatNearestVertex *rec, real32 radiusSquared, polygonobject *object,
                                                 sdword index, CatMeshObjectState *slots,
                                                 void (*callback)(vertexinfo *, CatVertexState *, void *), void *data)
{
    polygonobject *daughter;
    sdword next = index + 1;

    for (daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
    {
        next = cat_meshObjectForVerticesInRadiusR(rec, radiusSquared, daughter, next, slots, callback, data);
    }
    cat_meshObjectForVerticesInRadius(rec, radiusSquared, object, index, slots, callback, data);
    return next;
}

/*-----------------------------------------------------------------------------
    Name        : cat_meshBulletHitGlowCB (Cataclysm 0x004b5820, unnamed in the export)
    Description : The bullet hit's per-vertex callback: a vertex whose info has flag 0x01 is brought up to date,
                  gains *data of hit glow, and its info entry gets flag 0x02 (shared by every ship of that mesh).
    Inputs      : info, state - the vertex; data - a real32 *, the glow to add (1.0)
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void cat_meshBulletHitGlowCB(vertexinfo *info, CatVertexState *state, void *data)
{
    if (info->flags & 0x01)
    {
        cat_shGlowUpdate(state, (udword *)info);
        state->hitGlow += *(real32 *)data;
        info->flags |= 0x02;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_meshApplyBulletHit (Cataclysm 0x004b52d0)
    Description : For each LOD level of a ship's per-vertex mesh state: finds the mesh vertex nearest to a world
                  point and calls a callback on it and on every vertex within a radius of it.
    Inputs      : hit - the world point; radiusSquared; ship; callback, data - called as callback(info, state, data)
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void cat_meshApplyBulletHit(vector *hit, real32 radiusSquared, Ship *ship,
                            void (*callback)(vertexinfo *, CatVertexState *, void *), void *data)
{
    lodinfo *lodInfo = ship->staticinfo->common.LOD;
    hmatrix shipToWorld, objectToWorld;
    CatNearestVertex rec;
    CatMeshObjectState *slots;
    polygonobject *object, *daughter;
    sdword level, next;

    hmatMakeHMatFromMat(&shipToWorld, &ship->rotatable.rotinfo.coordsys);
    shipToWorld.m14 = ship->obj.posinfo.position.x;
    shipToWorld.m24 = ship->obj.posinfo.position.y;
    shipToWorld.m34 = ship->obj.posinfo.position.z;
    for (level = 0; level < lodInfo->nLevels; level++)
    {
        rec.info = NULL;
        rec.state = NULL;
        rec.distSquared = 0.0f;
        if (ship->obj.catMeshState == NULL)
        {
            break;
        }
        slots = ship->obj.catMeshState->level[level];
        if (slots == NULL)
        {                                                   // HWI: the exe walks every level as a mesh; a non-mesh
            continue;                                       // level has no slots here
        }
        //every top-level object starts at slot 0 and its daughters at 1 (0x004b53d6, 0x004b540b), as in the exe
        for (object = &((meshdata *)lodInfo->level[level].pData)->object[0]; object != NULL; object = object->pSister)
        {
            hmatMultiplyHMatByHMat(&objectToWorld, &shipToWorld, &object->localMatrix);
            for (next = 1, daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
            {
                next = cat_meshObjectNearestVertexToHitR(hit, &objectToWorld, daughter, next, slots, &rec);
            }
            cat_meshObjectNearestVertexToHit(hit, &objectToWorld, object, 0, slots, &rec);
        }
        if (rec.info == NULL)
        {                                                   // HWI: a mesh with no vertices (the exe would call the
            continue;                                       // callback with NULL)
        }
        callback(rec.info, rec.state, data);
        for (object = &((meshdata *)lodInfo->level[level].pData)->object[0]; object != NULL; object = object->pSister)
        {
            for (next = 1, daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
            {
                next = cat_meshObjectForVerticesInRadiusR(&rec, radiusSquared, daughter, next, slots, callback, data);
            }
            cat_meshObjectForVerticesInRadius(&rec, radiusSquared, object, 0, slots, callback, data);
        }
    }
}

/*=============================================================================
    CAT (M2-R5 render): where a line meets a ship's mesh (cat_meshLineIntersect 0x004b5910 with
    cat_meshObjectLineIntersect 0x004b55c0 / cat_meshObjectLineIntersectR 0x004b5860). The exe keeps its own
    40-deep CPU matrix stack (0x00a4b9d8, current matrix 0x00a4c400, depth 0x00a4c4d8, multiplied as GL does by
    cat_meshMatrixStackMult 0x004b5520); the matrices are passed down here instead. Each object's polygons are
    tested in object space (the line brought in with the inverse rotation), and a hit goes back to world space.
=============================================================================*/
//cat_meshObjectLineIntersect 0x004b55c0: the line start..end against each polygon of one object; a nearer hit, or the
//first one, sets *found, *bestT and *hit
static void cat_meshObjectLineIntersect(vector *start, vector *end, polygonobject *object, hmatrix *objectToWorld,
                                        vector *hit, real32 *bestT, sdword *found)
{
    matrix rotation;
    vector delta, localStart, localEnd, localHit;
    polyentry *poly;
    real32 t;
    sdword i;

    rotation.m11 = objectToWorld->m11; rotation.m21 = objectToWorld->m21; rotation.m31 = objectToWorld->m31;
    rotation.m12 = objectToWorld->m12; rotation.m22 = objectToWorld->m22; rotation.m32 = objectToWorld->m32;
    rotation.m13 = objectToWorld->m13; rotation.m23 = objectToWorld->m23; rotation.m33 = objectToWorld->m33;
    delta.x = start->x - objectToWorld->m14;
    delta.y = start->y - objectToWorld->m24;
    delta.z = start->z - objectToWorld->m34;
    matMultiplyVecByMat(&localStart, &delta, &rotation);
    delta.x = end->x - objectToWorld->m14;
    delta.y = end->y - objectToWorld->m24;
    delta.z = end->z - objectToWorld->m34;
    matMultiplyVecByMat(&localEnd, &delta, &rotation);
    for (i = 0; i < object->nPolygons; i++)
    {
        poly = &object->pPolygonList[i];
        if (cat_collLineTriangleIntersect(&localStart, &localEnd, (vector *)&object->pVertexList[poly->iV0],
                                          (vector *)&object->pVertexList[poly->iV1],
                                          (vector *)&object->pVertexList[poly->iV2],
                                          (vector *)&object->pNormalList[poly->iFaceNormal], &t) &&
            (t < *bestT || *found == 0))
        {
            *found = 1;
            *bestT = t;
            localHit.x = (localEnd.x - localStart.x) * t + localStart.x;
            localHit.y = (localEnd.y - localStart.y) * t + localStart.y;
            localHit.z = (localEnd.z - localStart.z) * t + localStart.z;
            matMultiplyMatByVec(hit, &rotation, &localHit);
            hit->x = objectToWorld->m14 + hit->x;
            hit->y = objectToWorld->m24 + hit->y;
            hit->z = objectToWorld->m34 + hit->z;
        }
    }
}

//cat_meshObjectLineIntersectR 0x004b5860: the daughters first, then the object
static void cat_meshObjectLineIntersectR(vector *start, vector *end, polygonobject *object, hmatrix *parentToWorld,
                                         vector *hit, real32 *bestT, sdword *found)
{
    polygonobject *daughter;
    hmatrix objectToWorld;

    hmatMultiplyHMatByHMat(&objectToWorld, parentToWorld, &object->localMatrix);
    for (daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
    {
        cat_meshObjectLineIntersectR(start, end, daughter, &objectToWorld, hit, bestT, found);
    }
    cat_meshObjectLineIntersect(start, end, object, &objectToWorld, hit, bestT, found);
}

/*-----------------------------------------------------------------------------
    Name        : cat_meshLineIntersect (Cataclysm 0x004b5910)
    Description : Where the line start..end (beyond start, not limited to end) meets a ship's mesh at a LOD level
                  (the last level if past it; p1Outpost always uses level 4, 0x00885d44). The nearest hit of
                  each top-level object is kept, and a later top-level object's hit replaces an earlier one (the exe
                  resets the best parameter to 1e21 per top-level object, 0x004b59fa).
    Inputs      : ship, level, start, end
    Outputs     : hit - the world point
    Return      : nonzero if the line meets the mesh
----------------------------------------------------------------------------*/
sdword cat_meshLineIntersect(Ship *ship, sdword level, vector *start, vector *end, vector *hit)
{
    lodinfo *lodInfo = ship->staticinfo->common.LOD;
    hmatrix shipToWorld, objectToWorld;
    polygonobject *object, *daughter;
    real32 bestT;
    sdword found = 0;

    if (ship->shiptype == p1Outpost)
    {
        level = 4;
    }
    if (level >= lodInfo->nLevels)
    {
        level = lodInfo->nLevels - 1;
    }
    hmatMakeHMatFromMat(&shipToWorld, &ship->rotatable.rotinfo.coordsys);
    shipToWorld.m14 = ship->obj.posinfo.position.x;
    shipToWorld.m24 = ship->obj.posinfo.position.y;
    shipToWorld.m34 = ship->obj.posinfo.position.z;
    for (object = &((meshdata *)lodInfo->level[level].pData)->object[0]; object != NULL; object = object->pSister)
    {
        bestT = 1e21f;
        hmatMultiplyHMatByHMat(&objectToWorld, &shipToWorld, &object->localMatrix);
        for (daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
        {
            cat_meshObjectLineIntersectR(start, end, daughter, &objectToWorld, hit, &bestT, &found);
        }
        cat_meshObjectLineIntersect(start, end, object, &objectToWorld, hit, &bestT, &found);
    }
    return found;
}

/*-----------------------------------------------------------------------------
    Name        : meshBindingListRecurse
    Description : Mesh-hierarchy recursion function to handle the creation of
                   mesh hierarchy binding lists.
    Inputs      : bindingList - list we're creating.
                  index - current entry in this list to set
                  object - current object we're setting
    Outputs     : stores the a pointer to object and all it's children in the
                    binding list in an order appropriate for rendering.
    Return      : new index into the list
----------------------------------------------------------------------------*/
sdword meshBindingListRecurse(mhbinding *bindingList, sdword index, polygonobject *object, ubyte parentIndex)
{
    polygonobject *daughter;
    sdword myIndex = index;                                 //CAT

    bindingList[index].object = object;                     //save the link to this object
    bindingList[index].function = NULL;                     //NULL means no function
    bindingList[index].parentIndex = parentIndex;           //CAT: parent binding index (meshBindingListRecurse 0x004b47b0)
    index++;
    for (daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
    {                                                               //service all children first
        index = meshBindingListRecurse(bindingList, index, daughter, (ubyte)myIndex);//CAT: children's parent is this binding
    }
    return(index);
}

/*-----------------------------------------------------------------------------
    Name        : meshBindingListCreate
    Description : Create a binding list for a mesh
    Inputs      : mesh - pointer to mesh to create binding list for
    Outputs     : Allocates and creates a binding list for the specified mesh.
                    Only the polyobject *will be set in each binding structure.
                  Order of structure reflects order to render in.
    Return      : Pointer to new binding list.
----------------------------------------------------------------------------*/
mhbinding *meshBindingListCreate(meshdata *mesh)
{
    sdword index = 0;
    polygonobject *sister;
    mhbinding *bindingList;

    bindingList = memAlloc(mesh->nPolygonObjects * sizeof(mhbinding), "MeshBindingList", NonVolatile);
    for (sister = &mesh->object[0]; sister != NULL; sister = sister->pSister)
    {                                                       //search all the base objects
        index = meshBindingListRecurse(bindingList, index, sister, 0xff);  //CAT: top-level objects have no parent (meshBindingListCreate 0x004b4840)
    }
#if MESH_ERROR_CHECKING
    if (index != mesh->nPolygonObjects)
    {
        dbgFatalf(DBG_Loc, "index %d != mesh->nPolygonObjects %d for '%s'.", index, mesh->nPolygonObjects, mesh->name);
    }
#endif
    return(bindingList);
}

/*-----------------------------------------------------------------------------
    Name        : meshBindingListDelete
    Description : Free memory associated with a binding list.
    Inputs      : list - list to delete
    Outputs     : just memFree's it
    Return      :
----------------------------------------------------------------------------*/
void meshBindingListDelete(mhbinding *list)
{
    memFree(list);
}

/*-----------------------------------------------------------------------------
    Name        : meshBindingFindByName
    Description : Find a named mesh binding.
    Inputs      : startBinding - starting binding to search from, very start if NULL
                  list - base of binding list
                  mesh - mesh to search in.
                  name - name of binding to search for.
    Outputs     : before returning the binding, sets a pointer to the source
                    poly object.
    Return      : pointer to binding whose name matches or NULL if no match
----------------------------------------------------------------------------*/
mhbinding *meshBindingFindByName(mhbinding *startBinding, mhbinding *list, meshdata *mesh, char *name)
{
    sdword count = mesh->nPolygonObjects;
    if (startBinding != NULL)
    {                                                       //start at either start of list or starting binding as referenced
        startBinding++;                                     //start on entry _after_ previous find
        count -= startBinding - list;                       //less bindings to search
        list = startBinding;                                //new list start
    }

    while (count > 0)
    {                                                       //search rest of list
        if (list->object->pName != NULL)
        {
            if (strcasecmp(list->object->pName, name) == 0)  // CAT: case-insensitive (meshBindingFindByName 0x004b4900: _stricmp)
            {                                               //if names match
                return(list);                               //all done
            }
        }
        list++;                                             //update pointer/counter
        count--;
    }
    return(NULL);
}

/*-----------------------------------------------------------------------------
    Name        : meshHierarchyWalkR
    Description : Recursion function for meshHierarchyWalk
    Inputs      : object - object to recursively walk
                  cbMesh - callback to call
    Outputs     :
    Return      : TRUE means to stop walking the list
----------------------------------------------------------------------------*/
bool32 meshHierarchyWalkR(meshdata *mesh, polygonobject *object, meshcallback preCallback, meshcallback postCallback)
{
    polygonobject *daughter;

    if (preCallback)
    {
        if (preCallback(mesh, object, (udword)object->iObject))
        {
            return(TRUE);
        }
    }
    for (daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
    {
        if (meshHierarchyWalkR(mesh, daughter, preCallback, postCallback))
        {
            return(TRUE);
        }
    }
    if (postCallback)
    {
        if (postCallback(mesh, object, (udword)object->iObject))
        {
            return(TRUE);
        }
    }
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : meshHierarchyWalk
    Description : Walk a mesh hierarchy in the order it will be rendered,
                    calling a user function for each mesh object.
    Inputs      : mesh - mesh object to walk
                  cbMesh - callback to call for each node in the hierarchy
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void meshHierarchyWalk(meshdata *mesh, meshcallback preCallback, meshcallback postCallback)
{
    polygonobject *object;

    for (object = &mesh->object[0]; object != NULL; object = object->pSister)
    {
        if (meshHierarchyWalkR(mesh, object, preCallback, postCallback))
        {
            break;
        }
    }
}

/*=============================================================================
    CAT: /catMeshLoadTest <listfile> (port diagnostic, not in the exe; docs/m2-r2-mesh.md)
=============================================================================*/
char catMeshSelfTestList[FL_Path] = "";             // CAT: list file of mesh paths, one per line; "" = test off

/*-----------------------------------------------------------------------------
    Name        : catMeshSelfTestCheck
    Description : CAT: port diagnostic. Checks a loaded mesh the way the renderer will use it: every polygon's
                  vertex, face-normal and material index, every vertex's normal index, the hierarchy links, the
                  object index and each local matrix's m44 (1.0). Counts the totals mesh_probe.py prints.
    Return      : number of objects that fail a check
----------------------------------------------------------------------------*/
static sdword catMeshSelfTestCheck(meshdata *mesh, sdword *nVerts, sdword *nPolys, sdword *nNorms, sdword *nGlow,
                                   sdword *nTextures)
{
    sdword iObject, i, nNormals, nMaterials, bad = 0;
    polygonobject *object, *first, *last;
    bool32 objectBad;

    nMaterials = mesh->nPublicMaterials + mesh->nLocalMaterials;
    first = &mesh->object[0];
    last = &mesh->object[mesh->nPolygonObjects - 1];
    *nVerts = *nPolys = *nNorms = *nGlow = *nTextures = 0;
    for (iObject = 0; iObject < mesh->nPolygonObjects; iObject++)
    {
        object = &mesh->object[iObject];
        nNormals = object->nFaceNormals + object->nVertexNormals;
        objectBad = FALSE;
        for (i = 0; i < object->nPolygons; i++)
        {
            polyentry *poly = &object->pPolygonList[i];
            if (poly->iV0 >= object->nVertices || poly->iV1 >= object->nVertices || poly->iV2 >= object->nVertices ||
                poly->iFaceNormal < 0 || poly->iFaceNormal >= nNormals || poly->iMaterial >= nMaterials)
            {
                objectBad = TRUE;
            }
        }
        for (i = 0; i < object->nVertices; i++)
        {
            if (object->pVertexList[i].iVertexNormal < 0 || object->pVertexList[i].iVertexNormal >= nNormals)
            {
                objectBad = TRUE;
            }
            if ((object->pVertexInfo[i].colourOffset & 0x00ffffff) != 0)
            {                                               // a vertex cat_shColourAddGlow tints
                (*nGlow)++;
            }
            if (!objectBad)
            {                                               // the colour path the renderer takes with enableCPV
                shColourSet0((vector *)&object->pNormalList[object->pVertexList[i].iVertexNormal], NULL,
                             &object->pVertexInfo[i].colourOffset);
            }
        }
        if ((object->pMother != NULL && (object->pMother < first || object->pMother > last)) ||
            (object->pDaughter != NULL && (object->pDaughter < first || object->pDaughter > last)) ||
            (object->pSister != NULL && (object->pSister < first || object->pSister > last)) ||
            object->localMatrix.m44 != 1.0f || object->iObject != (ubyte)iObject)
        {
            objectBad = TRUE;
        }
        if (objectBad)
        {
            bad++;
        }
        *nVerts += object->nVertices;
        *nPolys += object->nPolygons;
        *nNorms += nNormals;
    }
    for (i = 0; i < nMaterials; i++)
    {
        if (mesh->localMaterial[i].bTexturesRegistered)
        {
            (*nTextures)++;
        }
    }
    return bad;
}

/*-----------------------------------------------------------------------------
    Name        : catMeshSelfTestMad
    Description : CAT: port diagnostic. Loads a .mad mesh animation through madFileLoad, checks every object path
                  (name, at least one keyframe, times not decreasing, the animation names) and logs one
                  "meshtest: mad loaded ..." line.
    Return      : TRUE if the file failed a check
----------------------------------------------------------------------------*/
static bool32 catMeshSelfTestMad(char *fileName)
{
    madheader *header;
    sdword i, j, nKeyframes = 0, bad = 0;

    header = madFileLoad(fileName);
    for (i = 0; i < header->nObjects; i++)
    {
        madobjpath *path = &header->objPath[i];
        if (path->name == NULL || path->name[0] == '\0' || path->nKeyframes <= 0 ||
            path->nameCRC != crc16Compute((ubyte *)path->name, strlen(path->name)))
        {
            bad++;
            continue;
        }
        for (j = 1; j < path->nKeyframes; j++)
        {
            if (path->times[j] < path->times[j - 1])
            {
                bad++;
                break;
            }
        }
        nKeyframes += path->nKeyframes;
    }
    for (i = 0; i < header->nAnimations; i++)
    {
        if (header->anim[i].name == NULL || header->anim[i].endTime < header->anim[i].startTime)
        {
            bad++;
        }
    }
    dbgMessagef("meshtest: mad loaded %s objects=%d animations=%d keyframes=%d bad=%d",
                fileName, header->nObjects, header->nAnimations, nKeyframes, bad);
    madHeaderDelete(header);
    return bad != 0;
}

/*-----------------------------------------------------------------------------
    Name        : catMeshRunSelfTest
    Description : CAT: port diagnostic, not in the exe (command line /catMeshLoadTest <listfile>). Loads every mesh
                  named in the list file (paths as the game's archives name them, one per line) through meshLoad,
                  with paging off so each named file is the one loaded, checks it (catMeshSelfTestCheck), logs
                  one "meshtest: loaded ..." line per file and frees it again. A .mad path is loaded through
                  madFileLoad instead (catMeshSelfTestMad, "meshtest: mad loaded ...").
                  tools/formats/mesh_engine_check.py compares the log with tools/formats/mesh_probe.py.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void catMeshRunSelfTest(void)
{
    FILE *list;
    char line[FL_Path];
    char *end;
    meshdata *mesh;
    bool32 savedAllowPacking = mainAllowPacking;
    sdword nFiles = 0, nBad = 0, objectsBad;
    sdword nVerts, nPolys, nNorms, nGlow, nTextures;

    list = fopen(catMeshSelfTestList, "rt");
    if (list == NULL)
    {
        dbgMessagef("meshtest: cannot open list file %s", catMeshSelfTestList);
        return;
    }
    dbgMessagef("meshtest: self-test start, list %s", catMeshSelfTestList);
    mainAllowPacking = FALSE;                               // load the named file itself, not its .peo
    while (fgets(line, sizeof(line), list) != NULL)
    {
        for (end = line + strlen(line); end > line && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' '); end--)
        {
            end[-1] = '\0';
        }
        if (line[0] == '\0' || line[0] == '#')
        {
            continue;
        }
        nFiles++;
        if (strlen(line) > 4 && strcasecmp(line + strlen(line) - 4, ".mad") == 0)
        {                                                   // a mesh animation (MeshAnim.c)
            if (catMeshSelfTestMad(line))
            {
                nBad++;
            }
            continue;
        }
        mesh = meshLoad(line);
        objectsBad = catMeshSelfTestCheck(mesh, &nVerts, &nPolys, &nNorms, &nGlow, &nTextures);
        dbgMessagef("meshtest: loaded %s ident=%s objects=%d materials=%d verts=%d polys=%d norms=%d glowverts=%d textures=%d bad=%d",
                    line, mesh->localSize == UDWORD_Max ? "RMF97ba" : "RMF99ba", mesh->nPolygonObjects,
                    mesh->nPublicMaterials + mesh->nLocalMaterials, nVerts, nPolys, nNorms, nGlow, nTextures, objectsBad);
        if (objectsBad != 0)
        {
            nBad++;
        }
        meshFree(mesh);
    }
    fclose(list);
    mainAllowPacking = savedAllowPacking;
    dbgMessagef("meshtest: self-test done, %d files, %d with bad objects", nFiles, nBad);
}

/*=============================================================================
    CAT (M2-R5 render): the Beast-stun edge lightning and the mimic's warped disguise
=============================================================================*/
#define CAT_MESH_EDGE_POLYS_DIV     10          // 0x00885d3c: a tenth of the polygons carry lightning
#define CAT_MESH_EDGE_STEP_DIV      48          // 0x00885d40

// one object's edge lightning: a run of polygons (a tenth, at least 2) that walks round the object over time, each
// polygon's first edge drawn as an electric bolt (the particle renderer's, cat_partRenderUnk1System 0x004de880)
static void cat_meshObjectRenderEdgeEffect(polygonobject *object)                          // 0x004b4a60
{
    Ship *ship = (Ship *)catShCurrentObject;
    ubyte lod = ship->obj.currentLOD;
    sdword lastLevel = ship->staticinfo->common.LOD->nLevels - 1;
    sdword polys, step, quarter, k = 0;
    udword start, end, i;
    sdword segments;
    particle p;
    vertexentry *a, *b;

    polys = object->nPolygons / CAT_MESH_EDGE_POLYS_DIV;
    if (polys < 2)
    {
        polys = 2;
    }
    step = object->nPolygons / CAT_MESH_EDGE_STEP_DIV;
    if (step == 0)
    {
        step = 1;
    }
    // CAT: the exe adds the ship's address (0x004b4ae4): a per-ship offset; the address here differs, as it does
    //      between two runs of the exe
    start = ((udword)lrint((double)(universe.totaltimeelapsed * 10.0f)) + (udword)(memsize)ship) * (udword)step %
            (udword)object->nPolygons;
    end = (udword)(((sdword)start + polys) % object->nPolygons);
    quarter = polys / 4;
    alodIncPolys(polys);
    for (i = start; i != end; )
    {
        polyentry *poly = &object->pPolygonList[i];
        a = &object->pVertexList[poly->iV0];
        b = &object->pVertexList[poly->iV1];
        memset(&p, 0, sizeof(p));       // CAT-FIX: the exe leaves the fields it does not set (and flags) as stack garbage
        p.lifespan = 1.0f;
        p.position.x = b->x;
        p.position.y = b->y;
        p.position.z = b->z;
        p.wVel.x = a->x - b->x;
        p.wVel.y = a->y - b->y;
        p.wVel.z = a->z - b->z;
        p.length = fsqrt(p.wVel.x * p.wVel.x + p.wVel.y * p.wVel.y + p.wVel.z * p.wVel.z);
        p.scale = p.scaleY = (lod == 0) ? 2.0f : 1.0f;
        p.illum = 1.0f;
        if (quarter < k && k < polys - quarter)
        {
            p.icolor[0] = 0.8f;
            p.icolor[1] = 0.8f;
            p.icolor[2] = 1.0f;
            p.turbulance = 0.8f;
        }
        else
        {
            p.icolor[0] = 0.5f;
            p.icolor[1] = 0.5f;
            p.icolor[2] = 0.8f;
            p.turbulance = 0.4f;
        }
        segments = (uword)(((lastLevel - (sdword)lod + 1) * 6) / lastLevel);
        p.segments = (uword)((segments < 2) ? 2 : segments);
        cat_partRenderEBoltSystem(1, &p, PART_WORLDSPACE);
        k++;
        i++;
        if ((sdword)i >= object->nPolygons)
        {
            i %= (udword)object->nPolygons;
        }
    }
}

static void cat_meshObjectRenderEdgeEffectR(polygonobject *object)                         // 0x004b4dc0
{
    polygonobject *child;

    glPushMatrix();
    glMultMatrixf((GLfloat *)&object->localMatrix);
    for (child = object->pDaughter; child != NULL; child = child->pSister)
    {
        cat_meshObjectRenderEdgeEffectR(child);
    }
    cat_meshObjectRenderEdgeEffect(object);
    glPopMatrix();
}

static mhlocalbinding *cat_meshObjectRenderEdgeEffectHierarchy(mhlocalbinding *binding, polygonobject *object)  // 0x004b4d60
{
    polygonobject *child;

    glPushMatrix();
    glMultMatrixf((GLfloat *)((binding->function == NULL) ? &object->localMatrix : &binding->matrix));
    binding++;
    for (child = object->pDaughter; child != NULL; child = child->pSister)
    {
        binding = cat_meshObjectRenderEdgeEffectHierarchy(binding, child);
    }
    cat_meshObjectRenderEdgeEffect(object);
    glPopMatrix();
    return binding;
}

void cat_meshRenderEdgeEffect(Ship *ship, meshdata *mesh)                                 // 0x004b4e90
{
    polygonobject *object, *child;

    (void)ship;
    for (object = &mesh->object[0]; object != NULL; object = object->pSister)
    {
        glPushMatrix();
        glMultMatrixf((GLfloat *)&object->localMatrix);
        for (child = object->pDaughter; child != NULL; child = child->pSister)
        {
            cat_meshObjectRenderEdgeEffectR(child);
        }
        cat_meshObjectRenderEdgeEffect(object);
        glPopMatrix();
    }
    catRndCounts.edgeEffects++;
}

void cat_meshRenderShipHierarchyEdgeEffect(Ship *ship, meshdata *mesh)                    // 0x004b4e00
{
    mhlocalbinding *binding = ship->bindings->localBinding[ship->obj.currentLOD];
    polygonobject *object, *child;

    // the stored matrices of the last render (the binding functions are not called again)
    for (object = &mesh->object[0]; object != NULL; object = object->pSister)
    {
        glPushMatrix();
        glMultMatrixf((GLfloat *)((binding->function == NULL) ? &object->localMatrix : &binding->matrix));
        binding++;
        for (child = object->pDaughter; child != NULL; child = child->pSister)
        {
            binding = cat_meshObjectRenderEdgeEffectHierarchy(binding, child);
        }
        cat_meshObjectRenderEdgeEffect(object);
        glPopMatrix();
    }
    catRndCounts.edgeEffects++;
}

// a vertex pushed toward the sphere of the given radius about the centre by the fade (cat_meshWarpVertex 0x004b5aa0)
static void cat_meshWarpVertex(GLfloat *out, vector *centre, vertexentry *v, real32 fade, real32 radius)
{
    real32 dx = v->x - centre->x, dy = v->y - centre->y, dz = v->z - centre->z;
    real64 d = sqrt((real64)(dx * dx + dy * dy + dz * dz));

    if (d != 0.0)
    {
        d = ((real64)radius - d) / d;
        out[0] = (real32)(d * (real64)dx) * fade + v->x;
        out[1] = (real32)(d * (real64)dy) * fade + v->y;
        out[2] = (real32)(d * (real64)dz * (real64)fade + (real64)v->z);
    }
    else
    {
        out[0] = v->x;
        out[1] = v->y;
        out[2] = v->z;
    }
}

// a normal turned toward the radial direction by the fade (cat_meshWarpNormal 0x004b5b70)
static void cat_meshWarpNormal(vector *out, vector *centre, normalentry *n, real32 fade)
{
    real32 keep = 1.0f - fade;
    real32 dx = centre->x - n->x, dy = centre->y - n->y, dz = centre->z - n->z;
    real64 d = sqrt((real64)(dx * dx + dy * dy + dz * dz));
    real64 len = sqrt((real64)(n->x * n->x + n->y * n->y + n->z * n->z));

    if ((real32)d != 0.0f)
    {
        len = len / (real64)(real32)d;
        out->x = (real32)(len * (real64)dx) * fade + keep * n->x;
        out->y = (real32)(len * (real64)dy) * fade + keep * n->y;
        out->z = (real32)(len * (real64)dz * (real64)fade + (real64)keep * (real64)n->z);
        vecNormalize(out);
    }
    else
    {
        out->x = n->x;
        out->y = n->y;
        out->z = n->z;
    }
}

// one object of a mimic's disguise, its vertices pushed onto a sphere (the warp) as it fades in: lit in software
// (shColourSet0) with the GL lighting off
void cat_meshObjectRenderWarped(struct CatMeshObjectState *slots, polygonobject *object, materialentry *materials,
                                vector *centre, real32 fade, real32 radius, sdword iColorScheme)   // 0x004b5c90
{
    vertexentry *vertexList = object->pVertexList;
    normalentry *normalList = object->pNormalList;
    polyentry *polygon = object->pPolygonList;
    sdword iPoly, currentMaterial = -1, k;
    GLfloat v[3][3];
    vector n;
    uword iv[3];

    (void)slots;
    glShadeModel(GL_SMOOTH);
    rndLightingEnable(FALSE);
    alodIncPolys(object->nPolygons);
    glBegin(GL_TRIANGLES);
    for (iPoly = 0; iPoly < object->nPolygons; iPoly++, polygon++)
    {
        if (polygon->iMaterial != currentMaterial)
        {
            glEnd();
            currentMaterial = polygon->iMaterial;
            cat_meshMaterialSwitch(&materials[currentMaterial], iColorScheme);
            if (bFade)
            {
                glEnable(GL_BLEND);
            }
            glBegin(GL_TRIANGLES);
        }
        iv[0] = polygon->iV0;
        iv[1] = polygon->iV1;
        iv[2] = polygon->iV2;
        switch (meshPolyMode)
        {
            case MPM_Flat:
            case MPM_Texture:
                cat_meshWarpNormal(&n, centre, &normalList[polygon->iFaceNormal], fade);
                shColourSet0(&n, NULL, NULL);
                for (k = 0; k < 3; k++)
                {
                    cat_meshWarpVertex(v[k], centre, &vertexList[iv[k]], fade, radius);
                }
                for (k = 0; k < 3; k++)
                {
                    if (meshPolyMode == MPM_Texture)
                    {
                        glTexCoord2f(k == 0 ? polygon->s0 : (k == 1 ? polygon->s1 : polygon->s2),
                                     k == 0 ? polygon->t0 : (k == 1 ? polygon->t1 : polygon->t2));
                    }
                    glVertex3fv(v[k]);
                }
                break;
            case MPM_Smooth:
            case MPM_SmoothTexture:
                for (k = 0; k < 3; k++)
                {
                    cat_meshWarpVertex(v[k], centre, &vertexList[iv[k]], fade, radius);
                }
                for (k = 0; k < 3; k++)
                {
                    cat_meshWarpNormal(&n, centre, &normalList[vertexList[iv[k]].iVertexNormal], fade);
                    shColourSet0(&n, MESH_VERTEX_STATE(iv[k]), &object->pVertexInfo[iv[k]].colourOffset);
                    if (meshPolyMode == MPM_SmoothTexture)
                    {
                        glTexCoord2f(k == 0 ? polygon->s0 : (k == 1 ? polygon->s1 : polygon->s2),
                                     k == 0 ? polygon->t0 : (k == 1 ? polygon->t1 : polygon->t2));
                    }
                    glVertex3fv(v[k]);
                }
                break;
            default:
                break;
        }
    }
    glEnd();
    glShadeModel(GL_SMOOTH);
    if (!usingShader) glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
    if (bFade)
    {
        glDisable(GL_BLEND);
    }
    rndLightingEnable(TRUE);
    catRndCounts.warped++;
}

static mhlocalbinding *cat_meshObjectRenderWarpedHierarchy(mhlocalbinding *binding, polygonobject *object,
                                                           materialentry *materials, vector *centre, real32 fade,
                                                           real32 radius, sdword iColorScheme)       // 0x004b6330
{
    polygonobject *daughter;
    hmatrix *m;

    glPushMatrix();
    if (binding->function != NULL)
    {
        binding->function(binding->flags, &object->localMatrix, &binding->matrix, binding->userData, binding->userID);
        m = &binding->matrix;
    }
    else
    {
        m = &object->localMatrix;
    }
    glMultMatrixf((GLfloat *)m);
    shPushLightMatrix(m);
    binding++;
    for (daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
    {
        binding = cat_meshObjectRenderWarpedHierarchy(binding, daughter, materials, centre, fade, radius, iColorScheme);
    }
    cat_meshObjectRenderWarped(NULL, object, materials, centre, fade, radius, iColorScheme);
    shPopLightMatrix();
    glPopMatrix();
    return binding;
}

void cat_meshRenderShipHierarchyWarped(struct CatMeshState *meshState, shipbindings *bindings, sdword currentLOD,
                                       meshdata *mesh, vector *centre, real32 fade, real32 radius,
                                       sdword iColorScheme)                                     // 0x004b6400
{
    mhlocalbinding *binding = bindings->localBinding[currentLOD];
    polygonobject *object;

    (void)meshState;
    meshCurrentMaterial = g_NoMatSwitch ? meshAlmostCurrentMaterial : meshCurrentMaterialDefault;
    for (object = &mesh->object[0]; object != NULL; object = object->pSister)
    {
        binding = cat_meshObjectRenderWarpedHierarchy(binding, object, mesh->localMaterial, centre, fade, radius,
                                                      iColorScheme);
    }
    if (bindings->postCallback != NULL)
    {
        bindings->postCallback(mesh, bindings, currentLOD);
    }
}
