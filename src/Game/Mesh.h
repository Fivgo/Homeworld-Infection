/*=============================================================================
    Name    : mesh.h
    Purpose : Definitions for loading/saving mesh files.

    Created 6/21/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___MESH_H
#define ___MESH_H

#include <stddef.h>         // CAT: offsetof for the Cataclysm mesh layout asserts below (meshLoad 0x004aec90)

#include "Color.h"
#include "Matrix.h"
#include "texreg.h"

/*=============================================================================
    Switches:
=============================================================================*/
#define MESH_FORCED_COLORS      0               //force the red and blue colors
#define MESH_LOAD_DUMMY_TEXTURE 0               //load in a dummy texture
#define MESH_DUMMY_ST_COORDS    0               //compute default s/t coords
#define MESH_MATERIAL_STATS     1               //display material statistics
#define MESH_HACK_TEAM_COLORS   0               //hack team colors
#define MESH_TEAM_COLORS        0               //enable auto-team coloring of certain surfaces
#define MESH_PRE_CALLBACK       0
#define MESH_SURFACE_NAME_DEBUG 0

#ifdef HW_BUILD_FOR_DEBUGGING

#define MESH_VERBOSE_LEVEL      1
#define MESH_ERROR_CHECKING     1               //general error checking
#define MESH_ANAL_CHECKING      1               //extra-anal error checking
#define MESH_RETAIN_FILENAMES   1               //keep a copy of the fileName
#define MESH_MORPH_DEBUG        0               //debug morph render code

#else

#define MESH_VERBOSE_LEVEL      0
#define MESH_ERROR_CHECKING     0               //general error checking
#define MESH_ANAL_CHECKING      1               //extra-anal error checking
#define MESH_RETAIN_FILENAMES   1               //keep a copy of the fileName
#define TC_ALLOW_PCX            1               //load .PCX file if layered format not about
#define TC_SPECIAL_DEBUG        1               //special debugging modes active
#define MESH_MORPH_DEBUG        0

#endif

/*=============================================================================
    Definitions
=============================================================================*/
#define MESH_FileID                     "RMF97ba"
#define MESH_FileID2                    "RMF99ba"
#define MESH_FileIDLength               8
#define MESH_Version                    (0x00000402)

#ifdef _WIN32
#define MESH_TextureSubDir              "Textures\\"
#else
#define MESH_TextureSubDir              "Textures/"
#endif

#define MDF_AnimatingTextureMap         1
#define MDF_Smoothing                   2
#define MDF_AllowFlatShading            4
#define MDF_2Sided                      8
#define MDF_BaseColor                   16
#define MDF_StripeColor                 32
#define MDF_SelfIllum                   64

//mesh polygon modes for rendering
#define MPM_Flat                        0
#define MPM_Texture                     1
#define MPM_Smooth                      2
#define MPM_SmoothTexture               3

//hierarchy flags for polygon objects
#define POF_UniqueObject                1
#define POF_DuplicateObject             2
#define POF_LocalChildObject            4
#define POF_GlobalChildObject           8
#define POF_LocalParentObject           16
#define POF_GlobalParentObject          32
#define POF_LocalSiblingObject          64
#define POF_GlobalSiblingObject         128
#define POF_Last_POF                    129

//hierarchy binding flags
#define HBF_Azimuth                     0x00000100  //can rotate azimuth
#define HBF_Declination                 0x00000200  //can rotate declination
#define HBF_Recoil                      0x00000400  //can recoil
#define HBM_UpdateFrequency             0x000000ff  //8 bits for update frequency
#define HB_UpdateFrequencyMax           0x00000100  //limit of update frequency

//polygon flags for fixing up uv seaming
#define PAC_S0_0  0x0001
#define PAC_S0_1  0x0002
#define PAC_T0_0  0x0004
#define PAC_T0_1  0x0008
#define PAC_S1_0  0x0010
#define PAC_S1_1  0x0020
#define PAC_T1_0  0x0040
#define PAC_T1_1  0x0080
#define PAC_S2_0  0x0100
#define PAC_S2_1  0x0200
#define PAC_T2_0  0x0400
#define PAC_T2_1  0x0800
#define PAC_FIXED 0x1000

/*=============================================================================
    Type definitions:
=============================================================================*/
//structures used for mesh files
typedef struct tagGeoFileHeader
{
    char    identifier[MESH_FileIDLength];  // File identifier.
    udword  version;                        // File version.
    char   *pName;                          // Offset to a file name.
    udword  __obsolete;                     // was: fileSize => File size (in bytes), not counting this header.
    udword  localSize;                      // Object size (in bytes).
    udword  nPublicMaterials;               // Number of public materials.
    udword  nLocalMaterials;                // Number of local materials.
    memsize oPublicMaterial;                //list of public materials
    memsize oLocalMaterial;                 //list of local materials
    udword  nPolygonObjects;                // Number of polygon objects.
    ubyte   reserved[24];                   // Reserved for future use.
}
GeoFileHeader;

typedef struct tagpolygon
{
    sdword  iFaceNormal;                    // Index into the face normal list.
    uword  iV0;                             // Index to V0 of the polygon.
    uword  iV1;                             // Index to V1 of the polygon.
    uword  iV2;                             // Index to V2 of the polygon.
    uword  iMaterial;                       // Index into material list.
    real32 s0;
    real32 t0;
    real32 s1;
    real32 t1;
    real32 s2;
    real32 t2;
    uword  flags;                           // Flags for this polygon.
    ubyte reserved[2];
}
polyentry;

typedef struct tagvertex
{
    real32 x;                               // X component of this vertex.
    real32 y;                               // Y component of this vertex.
    real32 z;                               // Z component of this vertex.
    sdword iVertexNormal;                   // Index into the point normal list.
}
vertexentry;

typedef struct tagnormal
{
    real32 x;                               // X component of this normal.
    real32 y;                               // Y component of this normal.
    real32 z;                               // Z component of this normal.
    ubyte  pad[4];                          // Reserved for later use.
}
normalentry;

//CAT: per-vertex info entry, new in Cataclysm's mesh files: one 16-byte entry per vertex, pointed to by
//polygonobject +0x1C (meshLoad 0x004aec90 fixes it up; meshObjectRender 0x004b0170 passes &pVertexInfo[iV] to
//shColourSet0 0x0050dfe0, which hands it to cat_shColourAddGlow 0x0050d7c0). Field names are ours (docs/formats/mesh.md).
typedef struct tagvertexinfo
{
    udword colourOffset;                    // CAT: bytes 0..2 = signed R,G,B offsets added to the lit colour when the low 24 bits are non-zero; byte 3 = 0xFF in the data (0x80 in 28 of 139,602 entries, shared lists counted once) [audit: census of all 1,729 files]
    udword flags;                           // CAT: 0, 1, 2, 3, 0x10, 0x12, 0x13 in the data [audit: all 1,729 files; 2 and 0x12 were missing]; read by cat_shGlowUpdate 0x0050d560 (bits 0x01/0x02 hit glow, 0x10<<i trail i team colour)
    udword reserved[2];                     // CAT: always 0 in the shipped data
}
vertexinfo;

typedef struct tagmaterialentry
{
    char  *pName;                           // Offset to name of material (may be a CRC32).
    color  ambient;                         // Ambient color information.
    color  diffuse;                         // Diffuse color information.
    color  specular;                        // Specular color information.
    real32 kAlpha;                          // Alpha blending information.
    trhandle *texture;                         // Pointer to texture information (or CRC32).
    uword  flags;                           // Flags for this material.
    ubyte  nFullAmbient;                    // Number of self-illuminating colors in texture.
    bool8  bTexturesRegistered;             // Set to TRUE when some textures have been registered.
    char   *textureNameSave;                // the name of the texture, after the texture has been registered
}
materialentry;

typedef struct polygonobject
{
    char *pName;                            // Name for animation.
    ubyte   flags;                          // General flags (see above)
    ubyte   iObject;                        // fixed up at load time so we know what object index we have when recursively processing
    uword   nameCRC;                        // 16-bit CRC of name (!!!!no room for 32 right now - make room next version)
    sdword  nVertices;                      // Number of vertices in vertex list for this object.
    sdword  nFaceNormals;                   // Number of face normals for this object.
    sdword  nVertexNormals;                 // Number of vertex normals for this object.
    sdword  nPolygons;                      // Number of polygons in this object.
    vertexentry *pVertexList;               // Offset to the vertex list in this object.
    vertexinfo  *pVertexInfo;               // CAT: new per-vertex info list at +0x1C, inserted before pNormalList (meshLoad 0x004aec90)
    normalentry *pNormalList;               // Offset to the normal list in this object.
    polyentry   *pPolygonList;              // Offset to the polygon list in this object.
    struct polygonobject *pMother;          // link to parent object
    struct polygonobject *pDaughter;        // link to child object
    struct polygonobject *pSister;          // link to sibling object
    hmatrix localMatrix;
}
polygonobject;

//structure of mesh file header when loaded
typedef struct
{
    char *name;
    udword localSize;                       //size required for each instance of this mesh
    sdword nPublicMaterials;                //number of shared materials
    sdword nLocalMaterials;                 //number of local materials
    sdword nPolygonObjects;                 //number of polygon objects in mesh file
    struct tagmaterialentry *localMaterial; //list of local materials
    struct tagmaterialentry *publicMaterial;//list of public materials
#if MESH_RETAIN_FILENAMES
    char *fileName;                         //for debugging
#endif
    polygonobject object[1];                //array of polygon object files
}
meshdata;

//hierarchy binding information
struct shipbindings;
typedef bool32 (*mhbindingfunction)(udword flags, hmatrix *startMatrix, hmatrix *matrix, void *data, smemsize ID);
typedef void (*mhhelperfunction)(meshdata *mesh, struct shipbindings *bindings, sdword currentLOD);
typedef struct
{
    mhbindingfunction function;             //function to call to get a new matrix
    void *userData;                         //pointer data to pass
    sdword userID;                          //integer data to pass
    udword flags;                           //flags on how this binding operates (see above)
    polygonobject *object;                  //object of this binding
    ubyte parentIndex;                      //CAT: index of the parent object's binding, 0xFF for a top-level object
                                            //(+0x14, the binding is 0x18 bytes: meshBindingListRecurse 0x004b47b0)
}
mhbinding;

typedef struct mhlocalbinding
{
    mhbindingfunction function;             //function to call to get a new matrix
    void *userData;                         //pointer data to pass
    smemsize userID;                          //integer data to pass
    udword flags;                           //flags on how this binding operates (see above)
    polygonobject *object;                  //object of this binding
    hmatrix matrix;
    struct mhlocalbinding *parent;          //CAT: the parent object's binding or NULL (+0x54, 0x58 bytes in the exe;
                                            //set by cat_univMeshBindingsLinkParents 0x00541ae0)
}
mhlocalbinding;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(mhbinding) == 0x18, "mhbinding is 0x18 bytes in cataclysm.exe");
_Static_assert(sizeof(mhlocalbinding) == 0x58, "mhlocalbinding is 0x58 bytes in cataclysm.exe");
#endif

//hierarchy information to be found in ship structure
typedef struct shipbindings
{
    sdword frequencyCounter;                //counts how often to update the hierarchy
#if MESH_PRE_CALLBACK
    mhhelperfunction preCallback;           //called before the hierarchy is rendered
#endif
    mhhelperfunction postCallback;          //called after the hierarchy is rendered
    //this is a list of per-lod binding lists.  Length is same as number of LOD's
    mhlocalbinding *localBinding[1];        //bindings custom to this ship.
}
shipbindings;

//mesh hierarchy walking callback
typedef bool32 (*meshcallback)(meshdata *mesh, polygonobject *object, sdword iObject);


typedef struct tagGeoFileHeader_disk
{
    char    identifier[MESH_FileIDLength];  // File identifier.
    udword  version;                        // File version.
    udword   pName;   //    *               // Offset to a file name.
    udword  fileSize;                       // File size (in bytes), not counting this header.
    udword  localSize;                      // Object size (in bytes).
    udword  nPublicMaterials;               // Number of public materials.
    udword  nLocalMaterials;                // Number of local materials.
    udword  oPublicMaterial;                //list of public materials
    udword  oLocalMaterial;                 //list of local materials
    udword  nPolygonObjects;                // Number of polygon objects.
    ubyte   reserved[24];                   // Reserved for future use.
}
GeoFileHeader_disk;

typedef struct polygonobject_disk
{
    udword  pName;    // *                        // Name for animation.
    ubyte   flags;                          // General flags (see above)
    ubyte   iObject;                        // fixed up at load time so we know what object index we have when recursively processing
    uword   nameCRC;                        // 16-bit CRC of name (!!!!no room for 32 right now - make room next version)
    sdword  nVertices;                      // Number of vertices in vertex list for this object.
    sdword  nFaceNormals;                   // Number of face normals for this object.
    sdword  nVertexNormals;                 // Number of vertex normals for this object.
    sdword  nPolygons;                      // Number of polygons in this object.
    udword pVertexList; //vertexentry *pVertexList;     // Offset to the vertex list in this object.
    udword pVertexInfo; //vertexinfo *pVertexInfo;      // CAT: new per-vertex info list at +0x1C (meshLoad 0x004aec90)
    udword pNormalList;//normalentry *pNormalList;     // Offset to the normal list in this object.
    udword pPolygonList; //polyentry   *pPolygonList;   // Offset to the polygon list in this object.
    udword pMother; // struct polygonobject *pMother;          // link to parent object
    udword pDaughter; //struct polygonobject *pDaughter;        // link to child object
    udword pSister; //struct polygonobject *pSister;          // link to sibling object
    hmatrix localMatrix;
}
polygonobject_disk;

typedef struct
{
    char *name;
    udword localSize;                       //size required for each instance of this mesh
    sdword nPublicMaterials;                //number of shared materials
    sdword nLocalMaterials;                 //number of local materials
    sdword nPolygonObjects;                 //number of polygon objects in mesh file
    struct tagmaterialentry *localMaterial; //list of local materials
    struct tagmaterialentry *publicMaterial;//list of public materials
#if MESH_RETAIN_FILENAMES
    char *fileName;                         //for debugging
#endif
    polygonobject object[1];                //array of polygon object files
}
meshdata_disk;

typedef struct tagmaterialentry_disk
{
    udword  pName;   // *                        // Offset to name of material (may be a CRC32).
    color  ambient;                         // Ambient color information.
    color  diffuse;                         // Diffuse color information.
    color  specular;                        // Specular color information.
    real32 kAlpha;                          // Alpha blending information.
    udword texture;  // *                       // Pointer to texture information (or CRC32).
    uword  flags;                           // Flags for this material.
    ubyte  nFullAmbient;                    // Number of self-illuminating colors in texture.
    bool8  bTexturesRegistered;             // Set to TRUE when some textures have been registered.
    udword   textureNameSave; // *               // the name of the texture, after the texture has been registered
}
materialentry_disk;


//CAT: the on-disk layout of Cataclysm's .geo/.peo files (docs/formats/mesh.md). meshLoad 0x004aec90 reads a 0x44-byte
//header and walks the polygon objects at a stride of 0x74 (HW1: 0x70); the other records are unchanged.
#define CAT_ASSERT_MESH_OFFSET(type, field, off) \
    _Static_assert(offsetof(type, field) == (off), #type "." #field " is at " #off " in Cataclysm mesh files")
_Static_assert(sizeof(GeoFileHeader_disk) == 0x44, "GeoFileHeader is 0x44 bytes in Cataclysm mesh files");
_Static_assert(sizeof(polygonobject_disk) == 0x74, "polygonobject is 0x74 bytes in Cataclysm mesh files (HW1 0x70)");
_Static_assert(sizeof(materialentry_disk) == 0x20, "materialentry is 0x20 bytes in Cataclysm mesh files");
_Static_assert(sizeof(vertexentry) == 0x10, "vertexentry is 0x10 bytes in Cataclysm mesh files");
_Static_assert(sizeof(vertexinfo) == 0x10, "vertexinfo is 0x10 bytes in Cataclysm mesh files");
_Static_assert(sizeof(normalentry) == 0x10, "normalentry is 0x10 bytes in Cataclysm mesh files");
_Static_assert(sizeof(polyentry) == 0x28, "polyentry is 0x28 bytes in Cataclysm mesh files");
CAT_ASSERT_MESH_OFFSET(polygonobject_disk, pVertexList, 0x18);
CAT_ASSERT_MESH_OFFSET(polygonobject_disk, pVertexInfo, 0x1C);
CAT_ASSERT_MESH_OFFSET(polygonobject_disk, pNormalList, 0x20);
CAT_ASSERT_MESH_OFFSET(polygonobject_disk, pPolygonList, 0x24);
CAT_ASSERT_MESH_OFFSET(polygonobject_disk, pMother, 0x28);
CAT_ASSERT_MESH_OFFSET(polygonobject_disk, pDaughter, 0x2C);
CAT_ASSERT_MESH_OFFSET(polygonobject_disk, pSister, 0x30);
CAT_ASSERT_MESH_OFFSET(polygonobject_disk, localMatrix, 0x34);
CAT_ASSERT_MESH_OFFSET(GeoFileHeader_disk, nPolygonObjects, 0x28);
#if defined(__i386__)
//CAT: ILP32 (the exe's own layout): the in-memory polygonobject IS the file record, meshdata's head is 0x20 bytes
//(meshLoad allocates fileSize - 0x24 = fileSize - 0x44 + 0x20). Checked by tools/m2_layout32.sh.
_Static_assert(sizeof(polygonobject) == 0x74, "polygonobject is 0x74 bytes in cataclysm.exe");
_Static_assert(offsetof(meshdata, object) == 0x20, "meshdata.object is at 0x20 in cataclysm.exe");
CAT_ASSERT_MESH_OFFSET(polygonobject, pVertexInfo, 0x1C);
CAT_ASSERT_MESH_OFFSET(polygonobject, pPolygonList, 0x24);
CAT_ASSERT_MESH_OFFSET(polygonobject, localMatrix, 0x34);
CAT_ASSERT_MESH_OFFSET(materialentry, texture, 0x14);
_Static_assert(sizeof(materialentry) == 0x20, "materialentry is 0x20 bytes in cataclysm.exe");
#endif

/*=============================================================================
    Data:
=============================================================================*/
#if MESH_MATERIAL_STATS
extern sdword meshTotalMaterials;
extern sdword meshMaterialChanges;
extern char meshMaterialStatsString[100];
extern bool32 usingShader;
extern sdword meshImmediate;                 // HWI-FEATURE(mesh-batching): 1 = the immediate-mode mesh path (/meshImmediate, console row)
extern bool32 meshImmediateSwitched;        // HWI-FEATURE(mesh-batching): /meshImmediate stands
void meshImmediateSwitch(void);             // HWI-FEATURE(mesh-batching): /meshImmediate
#endif //MESH_MATERIAL_STATS

#if MESH_MORPH_DEBUG
extern bool32 meshMorphDebug;
#endif

/*=============================================================================
    Macros:
=============================================================================*/
#define shipBindingsLength(n)   (sizeof(shipbindings) + sizeof(mhlocalbinding *) * (n - 1))

/*=============================================================================
    Functions:
=============================================================================*/
//startup the mesh module
void meshStartup(void);
//shutdown the mesh module
void meshShutdown(void);

//load in a mesh file
meshdata *meshLoad(char *fileName);
meshdata *meshSimulationMesh(meshdata *mesh);       // HWI-FEATURE(assets-models): looks only
void meshFree(meshdata *mesh);
void meshRecolorize(meshdata *mesh);
void meshFixupUV(meshdata* mesh);
bool32 meshPagedVersionExists(char* fileName);

//render a specific mesh
sdword meshRender(meshdata *mesh, sdword iColorScheme);
void meshMorphedObjectRender(
    polygonobject* object1, polygonobject* object2, polyentry *uvPolys,
    materialentry* materials, real32 frac, sdword iColorScheme);
void meshMorphedObjectRenderTex(polygonobject* object1, polygonobject* object2,
                                polyentry *uvPolys, materialentry* material,
                                real32 frac, sdword iColorScheme);
struct CatMeshState;
struct CatMeshObjectState;
//CAT: the ship's per-vertex mesh state is the new first argument (meshRenderShipHierarchy 0x004b4520)
void meshRenderShipHierarchy(struct CatMeshState *meshState, shipbindings *bindings, sdword currentLOD, meshdata *mesh, sdword iColorScheme);
//CAT: meshRender with a LOD's vertex-state slots (the exe's meshRender 0x004b4280 first argument)
sdword cat_meshRenderState(struct CatMeshObjectState *slots, meshdata *mesh, sdword iColorScheme);
//CAT: the bullet-hit glow (Mesh.c)
struct Ship;
struct CatVertexState;
void cat_meshBulletHitGlowCB(vertexinfo *info, struct CatVertexState *state, void *data);       // 0x004b5820
void cat_meshApplyBulletHit(vector *hit, real32 radiusSquared, struct Ship *ship,
                            void (*callback)(vertexinfo *, struct CatVertexState *, void *), void *data);  // 0x004b52d0
//CAT (M2-R5 render): where a line meets a ship's mesh (Mesh.c)
sdword cat_meshLineIntersect(struct Ship *ship, sdword level, vector *start, vector *end, vector *hit);  // 0x004b5910
sdword cat_meshObjectNearestVertex(polygonobject *object, vector *point);                      // 0x004b4ef0 (M2-R6 nis-fow-camera)
void meshObjectRender(polygonobject *object, materialentry *materials, sdword iColorScheme);
void meshObjectRenderTex(polygonobject *object, materialentry *material);

//make a material current
void meshCurrentMaterialDefault(materialentry *material, sdword iColorScheme);

//binding list stuff
mhbinding *meshBindingListCreate(meshdata *mesh);
void meshBindingListDelete(mhbinding *list);
mhbinding *meshBindingFindByName(mhbinding *startBinding, mhbinding *list, meshdata *data, char *name);
sdword meshBindingIndexFindByName(mhbinding *list, meshdata *mesh, char *name);
bool32 meshFindHierarchyMatrixByUserData(hmatrix *dest, hmatrix *destNC, mhbinding *bindings, void *userData);
void meshHierarchyWalk(meshdata *mesh, meshcallback preMesh, meshcallback postCallback);

//CAT: port diagnostic, not in the exe: /catMeshLoadTest <listfile> loads every mesh in the list (docs/m2-r2-mesh.md)
extern char catMeshSelfTestList[];
void catMeshRunSelfTest(void);
void catMeshTexLogSet(char *match);                 // HWI-TEST (M3-R3 mines): /catMeshTexLog <substring>
bool32 catMeshTexLogMatches(const char *name);      // HWI-TEST
void catMeshTexLogDraw(void *spaceobj, meshdata *mesh, sdword iColorScheme);  // HWI-TEST: a SpaceObj

//CAT: 8 fade channels (meshSetFade 0x004ae700); see Mesh.c
#define MESH_FADE_CHANNELS          8
extern bool32 g_SpecificPoly;
void meshSetFade(sdword channel, real32 fade);
//CAT (M2-R5 render): the exe's alternative fade material, the Beast-stun edge lightning, the mimic's warped disguise
void cat_meshCurrentMaterialAlt(materialentry *material, sdword iColorScheme);                  // 0x004afcb0
struct Ship;
void cat_meshRenderEdgeEffect(struct Ship *ship, meshdata *mesh);                               // 0x004b4e90
void cat_meshRenderShipHierarchyEdgeEffect(struct Ship *ship, meshdata *mesh);                  // 0x004b4e00
void cat_meshObjectRenderWarped(struct CatMeshObjectState *slots, polygonobject *object, materialentry *materials,
                                vector *centre, real32 fade, real32 radius, sdword iColorScheme);   // 0x004b5c90
void cat_meshRenderShipHierarchyWarped(struct CatMeshState *meshState, shipbindings *bindings, sdword currentLOD,
                                       meshdata *mesh, vector *centre, real32 fade, real32 radius,
                                       sdword iColorScheme);                                     // 0x004b6400
void meshSetSpecular(sdword mode, ubyte red, ubyte green, ubyte blue, ubyte alpha); //mode -1 == off

#endif
