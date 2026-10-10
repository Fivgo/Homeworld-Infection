/*=============================================================================
    Name    : particle.c
    Purpose : maintain and render particle systems

    Created 11/13/1997 by khent
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/
#ifndef ___PARTICLE_H
#define ___PARTICLE_H

#include <stddef.h>

#include "Color.h"
#include "Mesh.h"
#include "SpaceObj.h"
#include "texreg.h"
#include "Types.h"
#include "Vector.h"

void partFilter(bool32 on);

#ifdef HW_BUILD_FOR_DEBUGGING
#define PART_Debug
#define PART_Verbose    1
#else
#undef  PART_Debug
#define PART_Verbose    0
#endif

#define PART_NUM_ALTERNATES     128

#define PART_MONOSCALE          0x1
#define PART_ALPHA              0x2
#define PART_WORLDSPACE         0x4             //world space motion updating
#define PART_WORLDSPACEVELOCITY 0x8             //if the particle is in worldspace, use it's velocity in local space for heading

// CAT: two new particle system types (partHeaderSize 0x004e25a0 and partCreationHelper 0x004e2150 know 7 types):
//      5 = electric bolts (createEBolts, cat_etgCreateEBolts 0x00470420; 0x24-byte header with turbulance and
//      segments, rendered by cat_partRenderUnk1System 0x004de880), 6 = lens flares (createLensFlares,
//      cat_etgCreateLensFlares 0x00470480; point-system header, one lens flare source per particle). The names are
//      ours; Ghidra and CET call them PART_UNK1/PART_UNK2.
typedef enum {PART_POINTS, PART_CUBES, PART_LINES, PART_MESH, PART_BILLBOARD, PART_EBOLTS, PART_LENSFLARES} particleType;

#define partTypeof(p)   ((pointSystem*)(p))->t
#define partNumberof(p) ((pointSystem*)(p))->n

#define PART_CYLINDER   1
#define PART_SPHERE     2

#define PART_SPECULAR        1
#define PART_STIPPLE         2
#define PART_NODEPTHWRITE    4
#define PART_ADDITIVE        8
#define PART_PSEUDOBILLBOARD 16
#define PART_TRUEBILLBOARD   32
#define PART_XYZSCALE        64
#define PART_NOCOLORSCHEME   0                  //particle has no definite color scheme

typedef char particleSystem;
typedef particleSystem *psysPtr;

#define PART_LOOPSTART  1
#define PART_LOOP       2

typedef struct
{
    trhandle    tex;        //== -1 if end, stay at last frame
    ubyte       uv[2][2];
#ifdef HW_PTR_64
    ubyte       pad1[4];
#endif
    udword      flags;      //LOOPSTART = 1, LOOP = 2
#ifdef HW_PTR_64
    udword       pad2;
#endif
} texAnim;

typedef struct
{
    meshdata* mesh;         //NULL = end of anim
    udword    flags;        //LOOPSTART = 1, LOOP = 2
} meshAnim;

// CAT: a lens flare source (cat_rndLensFlareSourceAdd 0x0057baa0 / cat_rndLensFlareSourceRemove 0x0057ba90 keep them
//      on one list, rendered by cat_rndLensFlaresRender 0x0057bec0). Each particle of a PART_LENSFLARES system owns
//      one (partFillGenericParticles 0x004e0640 allocates 0x40 bytes, partUpdateSystem copies the particle position
//      into it, cat_partSystemFreeAttachments 0x004e2500 removes and frees it). Fields after the position are named
//      by their initial values only; their use is in the lens flare renderer (stage 3).
typedef struct catlensflaresource
{
    vector position;                //+0x00 world position of the source
    real32 brightness;              //+0x0c 1.0 at creation
    ubyte  enabled;                 //+0x10 1 at creation
    ubyte  state;                   //+0x11 1 at creation, 2 once on the list
    ubyte  visible;                 //+0x12 1 at creation
    ubyte  pad0;
    udword unknown14;               //+0x14
    udword visibility;              //+0x18 0 when added to the list
    udword unknown1c[4];            //+0x1c
    Node   link;                    //+0x2c link in the lens flare source list
} catlensflaresource;

//generic particle type.
//this is way too big
// CAT: Cataclysm's particle is 0x100 bytes (HW1 0xE0; every partModify* steps by 0x100). New: scaleY (+0x54, a second
//      scale for X/Y-scaled sprites), lengthClamp (+0x70), turbulance (+0x74) and segments (+0x78) for electric
//      bolts, lensFlare (+0x7c), effectVel (+0x98). Offsets in the comments are the exe's (ILP32), pinned below.
typedef struct
{
    //lifespan
    real32 lifespan;                //0x00
    real32 waitspan;        //this decs before lifespan
    //velocity
    real32 velLOF;                  //0x08
    real32 velR;
    real32 deltaVelLOF;
    real32 deltaVelR;
    //position
    vector rvec;                    //0x18
    vector position;        //next to icolor, better for interleaved vertex lists
    //appearance
    real32 icolor[4];               //0x30
    real32 deltaColor[4];           //0x40
    real32 scale;                   //0x50 (CAT: X scale; modifyScaleX)
    real32 scaleY;                  //0x54 CAT: Y scale (modifyScaleY; partModifyScale sets both)
    real32 deltaScale;              //0x58
    bool8  lit;                     //0x5c
    real32 illum;                   //0x60
    real32 deltaIllum;
    //line-specifics
    real32 length;                  //0x68
    real32 deltaLength;             //0x6c
    bool32 lengthClamp;             //0x70 CAT: length stops at 0 (modifyDeltaLength's 3rd argument; partUpdateSystem)
    real32 turbulance;              //0x74 CAT: electric-bolt turbulance (setTurbulance)
    uword  segments;                //0x78 CAT: electric-bolt segments (setSegments)
    struct catlensflaresource *lensFlare;   //0x7c CAT: PART_LENSFLARES only
    //sprite-specifics
    trhandle tex;                   //0x80
    real32 rot;                     //0x84
    real32 deltaRot;

    //worldspace particle stuff
    //render only uses position, so only
    //creationhelper and updatesystem need this
    vector wVel;        //LOFvel + Rvel                     0x8c
    vector effectVel;   //CAT: effect velocity at creation  0x98 (partFillGenericParticles, worldspace systems)
    vector wAccel;      //LOFaccel + Rvel                   0xa4

    //color bias
    real32 bias[3];

    //animating sprite stuff
    void* tstruct;
    real32 rate;
    bool8 loop;
    real32 currentFrame;

    //mesh thingz
    real32 tumble[3];
    real32 deltaTumble[3];
    meshdata *mesh;
    real32 meshRate;
    void* mstruct;
    real32 meshFrame;
    real32 exponent;
    ubyte colorScheme;              //0-7 or PART_NOCOLORSCHEME if none
    sbyte loopCount;

    //flags
    udword flags;                   //0xfc
} particle;
#if defined(__i386__)
// CAT: exe layout of a particle (tools/m2_layout32.sh evaluates these)
_Static_assert(sizeof(particle) == 0x100, "CAT: particle is 0x100 bytes in the exe");
_Static_assert(offsetof(particle, scaleY) == 0x54, "CAT: particle.scaleY at +0x54");
_Static_assert(offsetof(particle, lengthClamp) == 0x70, "CAT: particle.lengthClamp at +0x70");
_Static_assert(offsetof(particle, lensFlare) == 0x7c, "CAT: particle.lensFlare at +0x7c");
_Static_assert(offsetof(particle, tex) == 0x80, "CAT: particle.tex at +0x80");
_Static_assert(offsetof(particle, effectVel) == 0x98, "CAT: particle.effectVel at +0x98");
_Static_assert(offsetof(particle, bias) == 0xb0, "CAT: particle.bias at +0xb0");
_Static_assert(offsetof(particle, tstruct) == 0xbc, "CAT: particle.tstruct at +0xbc");
_Static_assert(offsetof(particle, mesh) == 0xe4, "CAT: particle.mesh at +0xe4");
_Static_assert(offsetof(particle, exponent) == 0xf4, "CAT: particle.exponent at +0xf4");
_Static_assert(offsetof(particle, flags) == 0xfc, "CAT: particle.flags at +0xfc");
_Static_assert(sizeof(catlensflaresource) == 0x3c, "CAT: lens flare source fits the exe's 0x40-byte allocation");
#endif

//common params to all particles in a point system
typedef struct
{
    particleType t;                 //common among all particle system types
    udword flags;
    bool8 isAlive;                  //so's this
    uword n;
    real32 lastUpdated;             //task time it was last updated
    real32 drag;
    real32 ang, angDelta;           //end of common data
} pointSystem;

//common params to all particles in a cube system
typedef struct
{
    particleType t;
    udword flags;
    bool8 isAlive;
    uword n;
    real32 lastUpdated;             //task time it was last updated
    real32 drag;
    real32 ang, angDelta;
} cubeSystem;

//common params to all particles in a line system
typedef struct
{
    particleType t;
    udword flags;
    bool8 isAlive;
    uword n;
    real32 lastUpdated;             //task time it was last updated
    real32 drag;
    real32 ang, angDelta;
} lineSystem;

//common params to all particles in a mesh system
typedef struct
{
    particleType t;
    udword flags;
    bool8 isAlive;
    uword n;
    real32 lastUpdated;             //task time it was last updated
    real32 drag;
    real32 ang, angDelta;
    trhandle tex;
    matrix* partMat;
    vector  partPos;
} meshSystem;

//common params to all particles in a billboarded texture system
typedef struct
{
    particleType t;
    udword flags;
    bool8 isAlive;
    uword n;
    real32 lastUpdated;             //task time it was last updated
    real32 drag;
    real32 ang, angDelta;
    //if a particle's own trhandle == 0, it grabs from the
    //global setting for the system (right here)
    trhandle tex;
    udword uv[2][2];
    vector position;
    vector lofvec;
    uword slices;
    matrix* partMat;
} billSystem;

// CAT: header of an electric-bolt system (PART_EBOLTS; partCreationHelper 0x004e2150 case 5: the common point-system
//      header, then pat.turbulance at +0x1c and pat.segments at +0x20; 0x24 bytes in all)
typedef struct
{
    particleType t;
    udword flags;
    bool8 isAlive;
    uword n;
    real32 lastUpdated;             //task time it was last updated
    real32 drag;
    real32 ang, angDelta;
    real32 turbulance;
    uword  segments;
} eboltSystem;

#if defined(__i386__)
_Static_assert(sizeof(pointSystem) == 0x1c, "CAT: point/cube/line/lens-flare header 0x1c (partHeaderSize)");
_Static_assert(sizeof(meshSystem) == 0x30, "CAT: mesh system header 0x30 (partHeaderSize)");
_Static_assert(sizeof(billSystem) == 0x50, "CAT: billboard system header 0x50 (partHeaderSize)");
_Static_assert(sizeof(eboltSystem) == 0x24, "CAT: electric-bolt system header 0x24 (partHeaderSize)");
#endif

extern matrix partEffectOwnerSystem;
extern vector partEffectOwnerPosition;
extern color  partEffectColor;
extern real32 partNLips;
//functions
void partStartup(void);
void partShutdown(void);

void partSetDefaults(void);
void partRenderSystem(psysPtr psys);
bool8 partUpdateSystem(psysPtr psys, real32 dt, vector* velvec);    //TRUE if system died, FALSE otherwise
psysPtr partCreateSystem(particleType t, udword n);
psysPtr partCreateSystemWithDelta(particleType t, udword n, udword delta);
psysPtr partCreateSphericalSystem(particleType t, udword n);
psysPtr partCreateSphericalSystemWithDelta(particleType t, udword n, udword delta);
udword partHeaderSize(psysPtr psys);
// CAT: removes and frees the lens flare sources of a PART_LENSFLARES system (cat_partSystemFreeAttachments 0x004e2500;
//      etgEffectDelete and etgDeleteEffectsOwnedBy call it before freeing a particle block)
void cat_partSystemFreeAttachments(psysPtr psys);

/*-----------------------------------------------------------------------------
    CAT: ETG callables. Cataclysm's ETG calls every function of its function table with ONE pointer to an argument
    array (etgFunctionCall 0x0046e7b0; ETG.h etgarg/etgcallable). These are those functions: args[0] is the first
    argument as the script wrote it (a particle system pointer for the modify* family), a float sits in the low
    4 bytes of its slot. They return 0 (the exe's are void; returning memsize makes the call through etgcallable
    well-defined). Value-argument functions below them (partSetColor, partSetDrag, ...) are not in the table and keep
    HW1's convention, as in the exe.
-----------------------------------------------------------------------------*/
//system modifiers
memsize partModifyTumble(memsize *args);            //psys, x, y, z (CAT: 0x004e25f0, MESH systems)
memsize partModifyExponent(memsize *args);          //psys, exponent
memsize partModifyMesh(memsize *args);              //psys, mesh
memsize partModifyLighting(memsize *args);          //psys, lit
memsize partModifyIllum(memsize *args);             //psys, illum
memsize partModifyDeltaIllum(memsize *args);        //psys, deltaIllum
memsize partModifyTexture(memsize *args);           //psys, tex, u0, v0, u1, v1
memsize partModifyScale(memsize *args);             //psys, scale (CAT: X and Y scale)
memsize cat_partModifyScaleX(memsize *args);        //psys, scale (CAT: 0x004e2b40)
memsize cat_partModifyScaleY(memsize *args);        //psys, scale (CAT: 0x004e2bc0)
memsize partModifyDeltaScale(memsize *args);        //psys, deltaScale
memsize partModifyDeltaLength(memsize *args);       //psys, deltaLength, lengthClamp (CAT: 3rd argument)
memsize partModifyLifespan(memsize *args);          //psys, lifespan
memsize partModifyLength(memsize *args);            //psys, length
memsize partModifyVelLOF(memsize *args);            //psys, velLOF
memsize partModifyDeltaVelLOF(memsize *args);       //psys, deltaVelLOF
memsize partModifyVelR(memsize *args);              //psys, velR
memsize cat_partModifyRotAngle(memsize *args);      //psys, rot (CAT: 0x004e2fd0)
memsize partModifyDeltaVelR(memsize *args);         //psys, deltaVelR
memsize cat_partModifyAlphaValue(memsize *args);    //psys, alpha (CAT: 0x004e30d0)
memsize partModifyColorC(memsize *args);            //psys, color
memsize partModifyDeltaColor(memsize *args);        //psys, dr, dg, db, da
memsize partModifyColorBias(memsize *args);         //psys, rb, gb, bb
memsize partModifyAddColor(memsize *args);          //psys, color

//for billboard systems
memsize partModifyAnimation(memsize *args);         //psys, tstruct
memsize partModifyFramerate(memsize *args);         //psys, rate
memsize partModifyMorph(memsize *args);             //psys, mstruct
memsize partModifyMorphFramerate(memsize *args);    //psys, meshRate
void partModifyLoopFlag(psysPtr psys, bool8 willLoop);
void partModifyBillPosition(psysPtr psys, vector* pos);

//flag modifiers
void partModifyDrag(psysPtr psys, real32 d);
void partModifySpecular(psysPtr psys, bool32 spec);
void partModifyStipple(psysPtr psys, bool32 stip);
void partModifyNoDepthWrite(psysPtr psys, bool32 noWrite);
void partModifyAdditiveBlends(psysPtr psys, bool32 add);
memsize partModifyPseudoBillboard(memsize *args);   //psys, bill (also the table's modifyTrueBillboard, as HW1)
void partModifyTrueBillboard(psysPtr psys, bool32 bill);
//0 - normal, 1 - additive, 2 - stipple
memsize partModifyAlphaMode(memsize *args);         //psys, mode
memsize partModifyColorScheme(memsize *args);       //psys, colorScheme

//callbacks
//delta values are assumed to be + or - the given delta,
//unless the given is -ve which implies a strictly -ve delta

memsize partSetIsWorldspace(memsize *args);
memsize partSetVelocityInWorldSpace(memsize *args);
void partSetWorldVel(vector* worldVel);

//these only make sense for billboard systems
void partSetAnimation(void* tstruct);
void partSetFramerate(real32 rate);
memsize partSetLoopFlag(memsize *args);
memsize partSetStartFrame(memsize *args);

void partSetCoordSys(matrix* mat);

memsize partSetSlices(memsize *args);       //for circles

void partSetPosition(vector *pos);  //used by PART_BILLBOARD systems

memsize partSetMeshdata(memsize *args);     //used by PART_MESH systems (CAT: 0x004e3b80, setMesh's callable)
//used by PART_BILLBOARD systems
memsize partSetTrHandle(memsize *args);     //tex, u0, v0, u1, v1 (CAT: setTexture's callable)

//PART_MESH systems use this, vals are angles in radians (default 0,0,0)
memsize partSetTumble(memsize *args);
void partSetTumbleDist(real32 dx, real32 dy, real32 dz);
memsize partSetDeltaTumble(memsize *args);  //rads/s
void partSetDeltaTumbleDist(real32 dx, real32 dy, real32 dz);

void partSetExponent(real32 exponent);

//morphing stuff
void partSetMorph(void* mstruct);
void partSetMorphFramerate(real32 meshRate);
void partSetMorphLoopCount(sdword loopCount);
memsize partSetMeshStartFrame(memsize *args);

void partSetDrag(real32 n);
memsize partSetXYZScale(memsize *args);
memsize partSetScale(memsize *args);        //scale refers to linewidth with PART_LINES
memsize partSetScaleDist(memsize *args);
memsize partSetDeltaScale(memsize *args);
memsize partSetDeltaScaleDist(memsize *args);
memsize partSetOffsetLOF(memsize *args);        //starting offset along lof
memsize partSetOffsetR(memsize *args);          //starting offset along r
memsize partSetOffsetTheta(memsize *args);      //ditto
memsize partSetOffsetArray(memsize *args);      //array of offset positions
memsize cat_partSetRandomOffsetArray(memsize *args);    //CAT: 0x004e3cc0, random offsets in a box
memsize partSetDeltaLOF(memsize *args);         //modifies offsetLOF (nothing to do with velocity)
memsize partSetDeltaLOFDist(memsize *args);
memsize partSetDeltaR(memsize *args);
memsize partSetDeltaRDist(memsize *args);
memsize partSetVelLOF(memsize *args);           //velocity along lof
memsize partSetVelLOFDist(memsize *args);       //distribution of vel along lof
memsize partSetVelR(memsize *args);             //velocity along r
memsize partSetVelRDist(memsize *args);         //velRDist
memsize partSetDeltaVelLOF(memsize *args);      //acceleration
memsize partSetDeltaVelLOFDist(memsize *args);
memsize partSetDeltaVelR(memsize *args);
memsize partSetDeltaVelRDist(memsize *args);

//angles in radians
memsize partSetAng(memsize *args);
memsize partSetAngDist(memsize *args);
memsize partSetAngDelta(memsize *args);
memsize partSetAngDeltaDist(memsize *args);

void partSetColorA(real32 r, real32 g, real32 b, real32 a);
void partSetColor(real32 r, real32 g, real32 b);
memsize partSetColorDist(memsize *args);
memsize partSetColorADist(memsize *args);
memsize partSetDeltaColor(memsize *args);
memsize partSetDeltaColorDist(memsize *args);
memsize partSetDeltaColorA(memsize *args);
memsize partSetDeltaColorADist(memsize *args);

memsize partSetLighting(memsize *args);
memsize partSetIllum(memsize *args);
memsize partSetIllumDist(memsize *args);
memsize partSetDeltaIllum(memsize *args);
memsize partSetDeltaIllumDist(memsize *args);
memsize partSetLifespan(memsize *args);
memsize partSetLifespanDist(memsize *args);
memsize partSetWaitspan(memsize *args);
memsize partSetWaitspanDist(memsize *args);

memsize partSetLength(memsize *args);
memsize cat_partSetTurbulance(memsize *args);   //CAT: 0x004e4210, electric bolts
memsize cat_partSetSegments(memsize *args);     //CAT: 0x004e4220, electric bolts
memsize partSetLengthDist(memsize *args);
memsize partSetDeltaLength(memsize *args);
memsize partSetDeltaLengthDist(memsize *args);

memsize partSetColorBias(memsize *args);
memsize partSetColorBiasDist(memsize *args);

void partSetSpecular(bool32 spec);
void partSetStipple(bool32 stip);
void partSetNoDepthWrite(bool32 noWrite);
void partSetAdditiveBlends(bool32 add);
memsize partSetPseudoBillboard(memsize *args);
memsize partSetTrueBillboard(memsize *args);
memsize partSetAlphaMode(memsize *args);        //0 - normal, 1 - additive, 2 - stippled
memsize partSetColorScheme(memsize *args);

void partCreateCallbackSet(void (*function)(sdword userValue, ubyte *userData), sdword userValue, ubyte *userData);

void partCircleSolid3(vector* centre, real32 radius, sdword nSlices, color c);

#endif
