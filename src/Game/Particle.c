/*=============================================================================
    Name    : particle.c
    Purpose : maintain and render particle systems

    Created 11/13/1997 by khent
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#define PART_TEST_MORPH 0

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <math.h>
#include <float.h>
#include "glinc.h"
#include "Types.h"
#include "Vector.h"
#include "Randy.h"
#include "Debug.h"
#include "Memory.h"
#include "Particle.h"
#include "render.h"
#include "Mesh.h"
#include "FastMath.h"
#include "mainrgn.h"
#include "Task.h"
#include "Universe.h"
#include "AutoLOD.h"
#include "Shader.h"

#if defined _MSC_VER
	#define isnan(x) _isnan(x)
#endif

#ifndef RUB
#define RUB(c) (ubyte)(c * 255.0f)
#endif

#define TEXX(X) ((real32)(X) / (real32)texWidth)
#define TEXY(Y) ((real32)(Y) / (real32)texHeight)

#define MIN2(a,b) ((a) < (b) ? (a) : (b))

/*=============================================================================
    Data:
=============================================================================*/

static sdword alternateIndex = 0;
typedef struct alternate_s
{
    trhandle trhandle;    //of the etg texture
    udword glhandle;    //of the alternate
} alternate_t;

static alternate_t alternates[PART_NUM_ALTERNATES];

static GLfloat illumSave[4];
static bool32 wasStippled;

//function called for each particle created
void (*partCreateCallback)(sdword userValue, ubyte *userData) = NULL;
sdword partCreateUserValue = 0;
ubyte *partCreateUserData = NULL;

sdword partAdvanceMeshMorph(meshSystem* psys, particle* p);
meshdata* partMeshNextMesh(meshSystem* psys, particle* p);

// CAT: the exe's particle template is 0x1C8 bytes at 0x00a50410 (partSetDefaults 0x004dc3a0 copies 0x72 dwords from
//      0x0088d300). New against HW1: effectVelocity (+0x0c, replaces HW1's retained partEffectVelocity pointer;
//      partSetWorldVel 0x004e3a00 copies the vector into both), lengthClamp/turbulance/segments (+0xa4..+0xac).
typedef struct
{
    vector  worldVel;
    vector  effectVelocity;     //CAT: velocity of the effect that creates the particles (copied into particle.effectVel)
    //misc
    udword  flags;
    real32  drag;
    real32  scale;
    real32  xScale, yScale, zScale;
    real32  scaleDist;
    real32  deltaScale;
    real32  deltaScaleDist;
    //position
    real32  offsetLOF;
    real32  offsetR;
    real32  offsetTheta;
    vector *offsetArray;
    real32  deltaLOF;
    real32  deltaLOFDist;
    real32  deltaR[2];
    real32  deltaRDist[2];
    //velocity
    real32  velLOF;
    real32  velLOFDist;
    real32  velR;
    real32  velRDist;
    real32  deltaVelLOF;
    real32  deltaVelLOFDist;
    real32  deltaVelR;
    real32  deltaVelRDist;
    real32  ang;
    real32  angDist;
    real32  angDelta;
    real32  angDeltaDist;
    //line thingz
    real32  length;
    real32  lengthDist;
    real32  deltaLength;
    real32  deltaLengthDist;
    bool32  lengthClamp;        //CAT: +0xa4, copied into particle.lengthClamp; partSetDeltaLength clears it
    real32  turbulance;         //CAT: +0xa8, electric bolts (setTurbulance)
    uword   segments;           //CAT: +0xac, electric bolts (setSegments)
    //appearance
    real32  icolor[4];             // RGBA
    real32  colorDist[4];
    real32  deltaColor[4];
    real32  deltaColorDist[4];
    bool8   lit;
    real32  illum;
    real32  illumDist;
    real32  deltaIllum;
    real32  deltaIllumDist;
    //lifespan
    real32  lifespan;
    real32  lifespanDist;
    real32  waitspan;
    real32  waitspanDist;
    //mesh
    meshdata *mesh;
    real32  exponent;
    real32  exponentDist;
    ubyte   colorScheme;
    sdword  loopCount;
    //texture
    trhandle tex;
    //billboard
    real32  position[3];
    matrix* mat;
    udword  slices;
    udword  uv[2][2];
    void*   tstruct;
    void*   mstruct;
    real32  rate;
    real32  meshRate;
    real32  startMeshFrame;
    bool8   loop;
    uword   startFrame;
    //mesh tumble attribs
    real32  tumble[3];
    real32  tumbleDist[3];
    real32  deltaTumble[3];
    real32  deltaTumbleDist[3];
    //color biasing
    real32  rbias;
    real32  gbias;
    real32  bbias;
    real32  rbiasDist;
    real32  gbiasDist;
    real32  bbiasDist;
    //flags
    bool32    specularOnly;
    bool32    stippleAlpha;
    bool32    noDepthWrite;
    bool32    additiveBlends;
    bool32    pseudoBillboard;
    bool32    trueBillboard;
} particleAttribs;

particleAttribs defaultParticleAttribs =
{
    {0.0f, 0.0f, 0.0f}, //worldVel
    {0.0f, 0.0f, 0.0f}, //CAT: effectVelocity
    0,                  //flags.
    1.0f,               //drag
    5.0f,               //scale [3]
    1.0f, 1.0f, 1.0f,   //XYZ scale
    0.0f,               //scaleDist
    0.0f,               //deltaScale
    0.0f,               //deltaScaleDist

    0.0f,               //offsetLOF
    0.0f,               //offsetR
    0.0f,               //offsetTheta
    NULL,               //offsetArray
    0.0f,               //deltaLOF
    0.0f,               //deltaLOFDist
    {0.0f, TWOPI},      //deltaR[]
    {0.0f, TWOPI},      //deltaRDist[]
    0.0f,               //velLOF
    0.0f,               //velLOFDist
    0.0f,               //velR
    0.0f,               //velRDist
    0.0f,               //deltaVelLOF
    0.0f,               //deltaVelLOFDist
    0.0f,               //deltaVelR
    0.0f,               //deltaVelRDist
    0.0f,               //ang
    0.0f,               //angDist
    0.0f,               //angDelta
    0.0f,               //angDeltaDist

    5.0f,               //length
    0.0f,               //lengthDist
    0.0f,               //deltaLength
    0.0f,               //deltaLengthDist
    FALSE,              //CAT: lengthClamp
    0.8f,               //CAT: turbulance (exe default 0.8, 0x0088d3a8) [audit: was 0x0088d3b8]
    16,                 //CAT: segments (exe default 16, 0x0088d3ac) [audit: was 0x0088d3bc]

    {1.0f, 1.0f, 1.0f, 1.0f},       //icolor
    {0.0f, 0.0f, 0.0f, 0.0f},       //colorDist
    {0.0f, 0.0f, 0.0f, 0.0f},       //deltaColor
    {0.0f, 0.0f, 0.0f, 0.0f},       //deltaColorDist
    FALSE,                          //lit
    0.0f,                           //illum
    0.0f,                           //illumDist
    0.0f,                           //deltaIllum
    0.0f,                           //deltaIllumDist

    2.0f,               //lifespan (s) [2]
    0.0f,               //lifespanDist
    0.0f,               //waitspan
    0.0f,               //waitspanDist

    NULL,               //meshdata
    -1.0f,              //exponent
    0.0f,               //exponentDist
    PART_NOCOLORSCHEME, //color scheme
    -1,                 //loop count
    TR_Invalid,         //trhandle

    {0.0f, 0.0f, 0.0f}, //position
    NULL,
    8,                  //slices
    {{0, 0},            //uv
     {1, 1}},
    NULL,               //tstruct
    NULL,               //mstruct
    10.0f,              //rate (f/s)
    5.0f,               //mesh framerate (f/s)
    0.0f,               //starting mesh frame
    TRUE,               //loopFlag
    0,                  //startFrame

    {0.0f, 0.0f, 0.0f}, //tumble angles (radians)
    {0.0f, 0.0f, 0.0f}, //tumble distribution
    {0.0f, 0.0f, 0.0f}, //deltaTumble (rads/s)
    {0.0f, 0.0f, 0.0f}, //deltaTumble distribution

    //color bias
    0.0f,               //red bias
    0.0f,               //green bias
    0.0f,               //blue bias
    0.0f,               //red bias distribution
    0.0f,               //green bias dist
    0.0f,               //blue bias dist

    //flags
    FALSE,              //specularOnly
    FALSE,              //stippleAlpha
    FALSE,              //noDepthWrite
    TRUE,               //additiveBlends
    FALSE,              //pseudoBillboard
    FALSE               //trueBillboard
};

static particleAttribs pat;

/*-----------------------------------------------------------------------------
    Name        : partStartup
    Description : startup the particle module
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void partStartup(void)
{
    partShutdown();
}

/*-----------------------------------------------------------------------------
    Name        : partFreeAlternates
    Description : frees "alternates"
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void partFreeAlternates(void)
{
    sdword i;

    if (alternateIndex != 0)
    {
#if PART_Verbose > 0
        dbgMessagef("freeing %d particle alternates", alternateIndex);
#endif
        for (i = 0; i < alternateIndex; i++)
        {
            alternates[i].trhandle = TR_InvalidHandle;
            if (alternates[i].glhandle != 0)
            {
                glDeleteTextures(1, &alternates[i].glhandle);
                alternates[i].glhandle = 0;
            }
        }

        alternateIndex = 0;
    }
}

/*-----------------------------------------------------------------------------
    Name        : partShutdown
    Description : shutdown the particle module.  frees "alternates"
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void partShutdown(void)
{
    partFreeAlternates();
}

/*-----------------------------------------------------------------------------
    Name        : partSetDefaults
    Description : reset particle attributes structure to default values
    Inputs      :
    Outputs     : pat is filled with defaultParticleAttribs
    Return      :
----------------------------------------------------------------------------*/
void partSetDefaults(void)
{
    memcpy(&pat, &defaultParticleAttribs, sizeof(particleAttribs));
}

#define RealClamp(r) \
        if ((r) < 0.0f) r = 0.0f; \
        else if ((r) > 1.0f) r = 1.0f;
#define RealClampNeg(r) \
        if ((r) < -1.0f) r = -1.0f; \
        else if ((r) > 1.0f) r = 1.0f;

static hmatrix particleHMatrix;
static matrix  shiprotmatrix;
static vector  shipposition;
// CAT: HW1 kept a pointer to the creating effect's velocity here (partEffectVelocity); Cataclysm copies the vector into
//      pat.effectVelocity and every particle (particle.effectVel), and the renderers read the particle's copy

matrix partEffectOwnerSystem;
vector partEffectOwnerPosition;
color  partEffectColor;
real32 partNLips;

//box helper
static void drawBox(GLfloat size, GLenum type, particle *p, bool32 alpha)
{
    static GLfloat n[6][3] =
    {
        {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, -1.0f}
    };
    static GLint faces[6][4] =
    {
        {0, 1, 2, 3},
        {3, 2, 6, 7},
        {7, 6, 5, 4},
        {4, 5, 1, 0},
        {5, 6, 2, 1},
        {7, 4, 0, 3}
    };
    GLfloat v[8][3];
    GLint i;

    v[0][0] = v[1][0] = v[2][0] = v[3][0] = -size / 2;
    v[4][0] = v[5][0] = v[6][0] = v[7][0] = size / 2;
    v[0][1] = v[1][1] = v[4][1] = v[5][1] = -size / 2;
    v[2][1] = v[3][1] = v[6][1] = v[7][1] = size / 2;
    v[0][2] = v[3][2] = v[4][2] = v[7][2] = -size / 2;
    v[1][2] = v[2][2] = v[5][2] = v[6][2] = size / 2;

    for (i = 0; i < 6; i++)
    {
        glBegin(type);
        if (alpha)
        {
            glNormal3fv(&n[i][0]);
            glColor4f(p->icolor[0], p->icolor[1], p->icolor[2], p->icolor[3]);
            glVertex3fv(&v[faces[i][0]][0]);

            glNormal3fv(&n[i][0]);
            glColor4f(p->icolor[0], p->icolor[1], p->icolor[2], p->icolor[3]);
            glVertex3fv(&v[faces[i][1]][0]);

            glNormal3fv(&n[i][0]);
            glColor4f(p->icolor[0], p->icolor[1], p->icolor[2], p->icolor[3]);
            glVertex3fv(&v[faces[i][3]][0]);

            glNormal3fv(&n[i][0]);
            glColor4f(p->icolor[0], p->icolor[1], p->icolor[2], p->icolor[3]);
            glVertex3fv(&v[faces[i][2]][0]);
        }
        else
        {
            glNormal3fv(&n[i][0]);
            glColor3f(p->icolor[0], p->icolor[1], p->icolor[2]);
            glVertex3fv(&v[faces[i][0]][0]);

            glNormal3fv(&n[i][0]);
            glColor3f(p->icolor[0], p->icolor[1], p->icolor[2]);
            glVertex3fv(&v[faces[i][1]][0]);

            glNormal3fv(&n[i][0]);
            glColor3f(p->icolor[0], p->icolor[1], p->icolor[2]);
            glVertex3fv(&v[faces[i][3]][0]);

            glNormal3fv(&n[i][0]);
            glColor3f(p->icolor[0], p->icolor[1], p->icolor[2]);
            glVertex3fv(&v[faces[i][2]][0]);
        }
        glEnd();
    }
}

//like primCircleSolid3, but w/alpha
void partCircleSolid3(vector *centre, real32 radius, sdword nSlices, color c)
{
    sdword index;
    GLfloat v[3];
    double theta;

    v[0] = centre->x;
    v[1] = centre->y;
    v[2] = centre->z;
    glBegin(GL_TRIANGLE_FAN);
    glColor4ub(colRed(c), colGreen(c), colBlue(c), colAlpha(c));
    glVertex3fv(v);
    for (index = 0, theta = 0.0; index < nSlices; index++)
    {
        v[0] = centre->x + (real32)(sin(theta)) * radius;
        v[1] = centre->y + (real32)(cos(theta)) * radius;
        theta += 2.0 * PI / (double)nSlices;
        glColor4ub(colRed(c), colGreen(c), colBlue(c), colAlpha(c));
        glVertex3fv(v);
    }
    v[0] = centre->x;
    v[1] = centre->y + radius;
    glColor4ub(colRed(c), colGreen(c), colBlue(c), colAlpha(c));
    glVertex3fv(v);
    glEnd();
}

/*-----------------------------------------------------------------------------
    Name        : partBillboardEnable
    Description : sets up sprite billboard mode for worldspace & effectspace systems
    Inputs      : v - position of the sprite
                  isWorldspace - TRUE or FALSE
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void partBillboardEnable(vector *v, bool32 isWorldspace)
{
    if (isWorldspace)
    {
        //enable billboarding
        rndBillboardEnable(v);
    }
    else
    {
        vector vec;

        //object -> world
        vecSubFrom(*v,shipposition);
        matMultiplyMatByVec(&vec, &shiprotmatrix, v);
        vecAddTo(vec,shipposition);
        //enable billboarding
        rndBillboardEnable(&vec);
    }
}

/*-----------------------------------------------------------------------------
    Name        : partBillboardDisable
    Description : disables billboarding
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void partBillboardDisable(void)
{
    //disable billboarding
    rndBillboardDisable();
}

/*-----------------------------------------------------------------------------
    Name        : storeIllum
    Description : saves GL illumination parameters (LIGHT_MODEL_AMBIENT)
    Inputs      :
    Outputs     : global illumSave contains parameters
    Return      :
----------------------------------------------------------------------------*/
void storeIllum(void)
{
    glGetFloatv(GL_LIGHT_MODEL_AMBIENT, illumSave);
}

/*-----------------------------------------------------------------------------
    Name        : handleIllum
    Description : alters LIGHT_MODEL_AMBIENT according to passed particle's vals
    Inputs      : p - the particle
    Outputs     : the lighting model may be modified
    Return      :
----------------------------------------------------------------------------*/
void handleIllum(particle *p)
{
    GLfloat illum[4];
    if (p->illum != 0.0f)
    {
        illum[0] = p->illum;
        illum[1] = p->illum;
        illum[2] = p->illum;
        illum[3] = 1.0f;
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, illum);
    }
}

/*-----------------------------------------------------------------------------
    Name        : restoreIllum
    Description : restores saved GL illumination parameters
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void restoreIllum(void)
{
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, illumSave);
}

/*-----------------------------------------------------------------------------
    Name        : partPerformHacking
    Description : sets up a billboard-friendly MODELVIEW for non-worldspace
                  sprite systems
    Inputs      : partMat - the coordinate system
                  particleTranslation - position in the space
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void partPerformHacking(matrix* partMat, vector particleTranslation)
{
    hmatrix mat;
    hvector hvec = {0.0f, 0.0f, 0.0f, 1.0f};

    //build a homogenous matrix
    hmatMakeHMatFromMat(&particleHMatrix, partMat);
    hmatPutVectIntoHMatrixCol4(particleTranslation, particleHMatrix);

    //save position globally
    shipposition = particleTranslation;
    //save rotation globally
    matGetMatFromHMat(&shiprotmatrix, &particleHMatrix);

    //obtain current modelview
    glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat*)&mat);
    glPushMatrix();
    //zero translation elements
    hmatPutHVectIntoHMatrixCol4(hvec, mat);
    //load rotation but not translation into modelview
    glLoadMatrixf((GLfloat*)&mat);
}

/*-----------------------------------------------------------------------------
    Name        : partFilter
    Description : enables bilinear filtering for textures without regard to the
                  global filter setting
    Inputs      : on - TRUE or FALSE (yes or no)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void partFilter(bool32 on)
{
    if (on)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
}

/*-----------------------------------------------------------------------------
    Name        : partSaturateAdd
    Description : adds then normalizes input colour values
    Inputs      : out - output array of colours
                  add - input array 0
                  base - input array 1
    Outputs     : out is filled w/ saturated in+base
    Return      :
----------------------------------------------------------------------------*/
void partSaturateAdd(real32* out, real32* add, real32* base)
{
    real32 max, scale;
    real32 in[3];
    sdword i;

    for (i = 0; i < 3; i++)
    {
        in[i] = base[i] + add[i];
    }

    max = in[0];
    if (in[1] > max) max = in[1];
    if (in[2] > max) max = in[2];
    scale = (max == 0.0f) ? 1.0f : (1.0f / max);

    for (i = 0; i < 3; i++)
    {
        out[i] = in[i] * scale;
    }
}

void partBindAlternate(trhandle tex)
{
    sdword i;
    udword alternate;
    ubyte* data;

    texreg* reg = trStructureGet(tex);
    if (!bitTest(reg->flags, TRF_Alpha))
    {
        //do nothing for non-RGBA textures
        return;
    }

#ifndef HW_ENABLE_GLES
    //check alternates
    for (i = 0; i < alternateIndex; i++)
    {
        if (alternates[i].trhandle == tex)
        {
            trClearCurrent();
            glBindTexture(GL_TEXTURE_2D, alternates[i].glhandle);
            return;
        }
    }

    //create a new alternate
    glGenTextures(1, &alternate);
    alternates[alternateIndex].trhandle = tex;
    alternates[alternateIndex++].glhandle = alternate;   //store alternate

    if (alternateIndex >= PART_NUM_ALTERNATES)
    {
        //FIXME: this is not an efficient queue
        partFreeAlternates();
    }

    // ... from the data of the old texture (ASSERT: currently bound)
    data = (ubyte*)memAlloc(4*reg->scaledWidth*reg->scaledHeight, "temp part alternate", Pyrophoric);

#ifndef __EMSCRIPTEN__
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
#else
    // webgl does not support glgetteximage so we have to use read pixels via framebuffer attachements
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reg->handle, 0);

    glReadPixels(0, 0, reg->scaledWidth, reg->scaledHeight, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
#endif

    for (i = 0; i < 4*reg->scaledWidth*reg->scaledHeight; i += 4)
    {
        data[i+0] = data[i+1] = data[i+2] = 200;
    }

    trClearCurrent();
    glBindTexture(GL_TEXTURE_2D, alternate);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 reg->scaledWidth, reg->scaledHeight,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    memFree(data);
#endif
}

/*-----------------------------------------------------------------------------
    Name        : partRenderBillSystem
    Description : render a billboard system
    Inputs      : n - number of particles
                  p - pointer to first particle
                  flags - misc flags
                  particleTranslation - position of the system, used for billboarding
                  uv - texture uv coordinates
                  bpart - system header pointer
                  isWorldspace - TRUE or FALSE
    Outputs     :
    Return      : number of live particles; kill the system if 0
----------------------------------------------------------------------------*/
udword partRenderBillSystem(udword n, particle* p, udword flags,
                            vector particleTranslation, udword uv[2][2],
                            billSystem* bpart, bool32 isWorldspace)
{
    udword i, hits;
    vector pos;
    color col;
    real32 sx;
    real32 sy;
    trhandle currentTex;
    udword texWidth = 0, texHeight = 0;
    static vector origin = {0.0f, 0.0f, 0.0f};
    texreg *reg;
    udword  slices;
    matrix* partMat;
    trhandle tex;
    real32 saturatedBias[3] = {0.0, 0.0, 0.0};
    real32 tmpColors[4] = {0.0, 0.0, 0.0, 0.0};
    sdword blended;

    slices = bpart->slices;
    partMat = bpart->partMat;
    tex = bpart->tex;

    currentTex = tex;

//    rndAdditiveBlends(FALSE);

    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glShadeModel(GL_SMOOTH);

    rndTextureEnvironment(RTE_Modulate);

    if (currentTex != TR_Invalid)
    {
        reg = trStructureGet(tex);
        texWidth = reg->diskWidth;
        texHeight = reg->diskHeight;
        rndTextureEnable(TRUE);
/*
        trMakeCurrent(tex);
        if (bitTest(reg->flags, TRF_Alpha))
        {
            glEnable(GL_BLEND);
            glDisable(GL_ALPHA_TEST);
            rndAdditiveBlends(FALSE);
        }
*/
    }

    for (i = hits = 0; i < n; i++, p++)
    {
        if (p->lifespan < 0.0f || p->waitspan > 0.0f)
            continue;
        else
            hits++;
        if (p->waitspan > 0.0f)
            continue;
        pos = p->position;
        if (!isWorldspace)
        {
            vecAddTo(pos, particleTranslation);
        }
        partBillboardEnable(&pos, isWorldspace);

        if (p->tstruct == NULL && p->tex != TR_Invalid)
        {
            currentTex = p->tex;
        }

        handleIllum(p);

        blended = 0;
        saturatedBias[0] = p->bias[0];
        saturatedBias[1] = p->bias[1];
        saturatedBias[2] = p->bias[2];

        if ((p->bias[0] + p->bias[1] + p->bias[2]) > 0.0f)
        {
            blended = 1;
        }

        if (p->icolor[3] == 1.0f)
        {
            glDisable(GL_BLEND);
        }
        else
        {
            glEnable(GL_BLEND);
        }
        tmpColors[0] = p->icolor[0];
        tmpColors[1] = p->icolor[1];
        tmpColors[2] = p->icolor[2];
        tmpColors[3] = p->icolor[3];

        if (currentTex == TR_Invalid)
        {
            col = colRGBA(colRealToUbyte(p->icolor[0]), colRealToUbyte(p->icolor[1]),
                          colRealToUbyte(p->icolor[2]), colRealToUbyte(p->icolor[3]));
            partCircleSolid3(&origin, p->scale, slices, col);
        }
        else
        {
            real32 cosTheta, sinTheta;

            rndTextureEnable(TRUE);

            trMakeCurrent(currentTex);
            reg = trStructureGet(tex);
            if (bitTest(reg->flags, TRF_Alpha))
            {
                glEnable(GL_BLEND);
                glDisable(GL_ALPHA_TEST);
                if (bitTest(p->flags, PART_ADDITIVE))
                {
                    rndAdditiveBlends(TRUE);
                }
                else
                {
                    rndAdditiveBlends(FALSE);
                }
            }
            partFilter(TRUE);

            cosTheta = (real32)cos((real64)p->rot);
            sinTheta = (real32)sin((real64)p->rot);

            // CAT: separate X and Y scales (partRenderBillSystem 0x004dcbf7: scale * 0.5 and scaleY * 0.5)
            sx = p->scale * 0.5f;
            sy = p->scaleY * 0.5f;

            if (p->rot == 0.0f)
            {
AGAIN0:
                glBegin(GL_TRIANGLE_STRIP);
                glColor4f(tmpColors[0], tmpColors[1], tmpColors[2], tmpColors[3]);
                glTexCoord2f(TEXX(uv[0][0]), TEXY(uv[0][1]));
                glVertex3f(-sx, -sy, 0.0f);

                glColor4f(tmpColors[0], tmpColors[1], tmpColors[2], tmpColors[3]);
                glTexCoord2f(TEXX(uv[1][0]), TEXY(uv[0][1]));
                glVertex3f(sx, -sy, 0.0f);

                glColor4f(tmpColors[0], tmpColors[1], tmpColors[2], tmpColors[3]);
                glTexCoord2f(TEXX(uv[0][0]), TEXY(uv[1][1]));
                glVertex3f(-sx, sy, 0.0f);

                glColor4f(tmpColors[0], tmpColors[1], tmpColors[2], tmpColors[3]);
                glTexCoord2f(TEXX(uv[1][0]), TEXY(uv[1][1]));
                glVertex3f(sx, sy, 0.0f);

                glEnd();
                if (blended)
                {
                    blended--;
                    rndAdditiveBlends(TRUE);
                    tmpColors[0] = saturatedBias[0];
                    tmpColors[1] = saturatedBias[1];
                    tmpColors[2] = saturatedBias[2];
                    tmpColors[3] = p->icolor[3];
                    //render this guy again, additively blending over the last
                    partBindAlternate(currentTex);
                    partFilter(TRUE);
                    goto AGAIN0;
                }
            }
            else
            {
AGAIN1:
                glBegin(GL_TRIANGLE_STRIP);
                glColor4f(tmpColors[0], tmpColors[1], tmpColors[2], tmpColors[3]);
                glTexCoord2f(TEXX(uv[0][0]), TEXY(uv[0][1]));
                glVertex3f((-sx*cosTheta) - (-sy*sinTheta),
                           (-sx*sinTheta) + (-sy*cosTheta),
                           0.0f);

                glColor4f(tmpColors[0], tmpColors[1], tmpColors[2], tmpColors[3]);
                glTexCoord2f(TEXX(uv[1][0]), TEXY(uv[0][1]));
                glVertex3f((sx*cosTheta) - (-sy*sinTheta),
                           (sx*sinTheta) + (-sy*cosTheta),
                           0.0f);

                glColor4f(tmpColors[0], tmpColors[1], tmpColors[2], tmpColors[3]);
                glTexCoord2f(TEXX(uv[0][0]), TEXY(uv[1][1]));
                glVertex3f((-sx*cosTheta) - (sy*sinTheta),
                           (-sx*sinTheta) + (sy*cosTheta),
                           0.0f);

                glColor4f(tmpColors[0], tmpColors[1], tmpColors[2], tmpColors[3]);
                glTexCoord2f(TEXX(uv[1][0]), TEXY(uv[1][1]));
                glVertex3f((sx*cosTheta) - (sy*sinTheta),
                           (sx*sinTheta) + (sy*cosTheta),
                           0.0f);

                glEnd();
                if (blended)
                {
                    blended--;
                    rndAdditiveBlends(TRUE);
                    tmpColors[0] = saturatedBias[0];
                    tmpColors[1] = saturatedBias[1];
                    tmpColors[2] = saturatedBias[2];
                    tmpColors[3] = p->icolor[3];
                    //render again, additively blending over previous
                    partBindAlternate(currentTex);
                    partFilter(TRUE);
                    goto AGAIN1;
                }
            }
        }

        partBillboardDisable();
    }
    rndTextureEnable(FALSE);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    rndAdditiveBlends(FALSE);
    return hits;
}

/*-----------------------------------------------------------------------------
    Name        : partMeshMaterialPrepare
    Description : Prepare to render a mesh in singl-texture more by setting up
                    material attributes.
    Inputs      : currentTex - handle of texture we're going to render in.
                  p - particle we're going to render.
                  material - first material of the mesh
                  alpha - enable GL_BLEND if alpha > 0 regardless of other settings
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void partMeshMaterialPrepare(particle *p, trhandle currentTex, materialentry *material, bool32 alpha)
{
    texreg *reg;
    GLfloat attribs[4];
    real32 ambientFactor;
    real32 specularFactor;
    real32 diffuseRed, otherRed;
    GLenum face = GL_FRONT_AND_BACK;  //!!!

    rndTextureEnable(TRUE);

    trMakeCurrent(currentTex);
    reg = trStructureGet(currentTex);
    if (bitTest(reg->flags, TRF_Alpha) || alpha)
    {
        glEnable(GL_BLEND);
        glDisable(GL_ALPHA_TEST);
        if (bitTest(p->flags, PART_ADDITIVE))
        {
            rndAdditiveBlends(TRUE);
        }
        else
        {
            rndAdditiveBlends(FALSE);
        }
    }
    partFilter(TRUE);
    if (p->lit)
    {                                           //if particle is lit
        //this is an attempt to extract the ambient and specular properties from
        //the materials of the mesh without actually using the ambient and
        //specular colors.  It should work in most cases.  It uses the ratio of
        //ambient/diffuse and ambient/specular reds in the first material of the mesh.
        diffuseRed = colUbyteToReal(colRed(material->diffuse));
        otherRed = colUbyteToReal(colRed(material->ambient));
        ambientFactor = otherRed / diffuseRed;
        otherRed = colUbyteToReal(colRed(material->specular));
        specularFactor = otherRed / diffuseRed;

        attribs[0] = p->icolor[0] * ambientFactor;
        attribs[1] = p->icolor[1] * ambientFactor;
        attribs[2] = p->icolor[2] * ambientFactor;
        attribs[3] = 1.0f;
        glMaterialfv(face, GL_AMBIENT, attribs);
        attribs[0] = p->icolor[0] * specularFactor;
        attribs[1] = p->icolor[1] * specularFactor;
        attribs[2] = p->icolor[2] * specularFactor;
        glMaterialfv(face, GL_SPECULAR, attribs);
        attribs[0] = p->icolor[0];
        attribs[1] = p->icolor[1];
        attribs[2] = p->icolor[2];
        attribs[3] = p->icolor[3];
        glMaterialfv(face, GL_DIFFUSE, attribs);
        rndLightingEnable(TRUE);
    }
    else
    {
        rndLightingEnable(FALSE);
    }
}

void calcBillboardMatrixWorld(matrix* mat, vector* pos, particle* p)
{
    //nothing here
}

void calcBillboardMatrixNotWorld(matrix* mat, vector* pos)
{
    //nothing here
}

void undoBillboardMatrix(void)
{
    //nothing here
}

/*-----------------------------------------------------------------------------
    Name        : calcPseudoBillboardMatrixWorld
    Description : Calculate a pseudo-billboard matrix for a world-space mesh particle
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void calcPseudoBillboardMatrixWorld(matrix* mat, vector* pos, particle* p, meshSystem* meshPart)
{
    vector velocity, veye, up, right;
    matrix matResult;
    hmatrix hmatResult;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    velocity = p->wVel;
    if (!bitTest(meshPart->flags, PART_WORLDSPACEVELOCITY))
    {
        vecSub(velocity, velocity, p->effectVel);       // CAT: the particle keeps the effect velocity (exe [esi+0x98], 0x004dd69a/0x004dd890/0x004ddbd0)
    }
    if (velocity.x == 0.0f && velocity.y == 0.0f && velocity.z == 0.0f)
    {
        velocity.z = 1.0f;
    }
    else
    {
        vecNormalize(&velocity);
    }

    vecSub(veye, p->position, mrCamera->eyeposition);
    vecNormalize(&veye);

    vecCrossProduct(up, veye, velocity);
    vecCrossProduct(right, velocity, up);
    vecNormalize(&right);
    vecNormalize(&up);

    matPutVectIntoMatrixCol1(up, matResult);
    matPutVectIntoMatrixCol2(right, matResult);
    matPutVectIntoMatrixCol3(velocity, matResult);

    hmatMakeHMatFromMat(&hmatResult, &matResult);

    glMultMatrixf((GLfloat*)&hmatResult);
}

void partInverselyTransform(vector* out, vector* in, matrix* mat, vector* localPos)
{
    vector  spacedIn;

    vecSub(spacedIn, *in, *localPos);
    //transform this way to perform "inverse" operation
    matMultiplyVecByMat(out, &spacedIn, mat);
}

/*-----------------------------------------------------------------------------
    Name        : calcPseudoBillboardMatrixNotWorld
    Description : Calculate a pseudo-billboard matrix for a local-space mesh particle
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void calcPseudoBillboardMatrixNotWorld(matrix* mat, vector* pos, particle* p, meshSystem* meshPart)
{
    vector  velocity, veye, up, right;
    vector  camera_effect, origin_world;
    matrix  matResult;
    hmatrix hmatResult;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    //obtain velocity vector in effectspace
    vecScalarMultiply(velocity, p->rvec, p->velR);
    if (!bitTest(meshPart->flags, PART_WORLDSPACEVELOCITY))
    {
        vecSub(velocity, velocity, p->effectVel);       // CAT: the particle keeps the effect velocity (exe [esi+0x98], 0x004dd69a/0x004dd890/0x004ddbd0)
    }
    if (velocity.x == 0.0f && velocity.y == 0.0f && velocity.z == 0.0f)
    {
        velocity.z = 1.0f;
    }
    else
    {
        vecNormalize(&velocity);
    }

    //obtain particle's worldspace position
    matMultiplyMatByVec(&origin_world, mat, &p->position);
    vecAddTo(origin_world, *pos);

    //obtain camera's position in effectspace
    partInverselyTransform(&camera_effect, &mrCamera->eyeposition, mat, &origin_world);
    //obtain veye
    vecSub(veye, p->position, camera_effect);
    vecNormalize(&veye);

    //obtain up vector from veye & velocity
    vecCrossProduct(up, veye, velocity);
    vecNormalize(&up);
    //obtain right vector from velocity & up
    vecCrossProduct(right, velocity, up);
    vecNormalize(&right);

    //form a coordinate system
    matPutVectIntoMatrixCol1(up, matResult);
    matPutVectIntoMatrixCol2(right, matResult);
    matPutVectIntoMatrixCol3(velocity, matResult);

    hmatMakeHMatFromMat(&hmatResult, &matResult);

    glMultMatrixf((GLfloat*)&hmatResult);
}

void undoPseudoBillboardMatrix(void)
{
    //ASSERT: MatrixMode == MODELVIEW
    glPopMatrix();
}

/*-----------------------------------------------------------------------------
    Name        : partMeshOrient
    Description : Create a matrix for the mesh
    Inputs      : p - particle to work with
                  bRescaleNormal - TRUE if normals are to be normalized (as in scaled meshes)
                  meshPart - pointer to mesh particle
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void partMeshOrient(particle* p, bool32 bRescaleNormal, meshSystem* meshPart)
{
    vector velocity;
    hmatrix velHMatrix;
    matrix velMatrix;
    real32 length, scale;

    if (bitTest(p->flags, PART_XYZSCALE))
    {
        hmatMakeHMatFromMat(&velHMatrix, &partEffectOwnerSystem/*meshPart->partMat*/);
        hmatPutVectIntoHMatrixCol4(partEffectOwnerPosition/*p->position*/, velHMatrix);
        glMultMatrixf((GLfloat*)&velHMatrix);
/*
        velocity.x = 0.0f;
        velocity.y = 0.0f;
        velocity.z = -1.0f;

        matCreateCoordSysFromHeading(&velMatrix, &velocity);
        hmatMakeHMatFromMat(&velHMatrix, &velMatrix);
        glMultMatrixf((GLfloat*)&velHMatrix);
*/
        hmatMakeRotAboutX(&velHMatrix, (real32)cos(DEG_TO_RAD(180.0)), (real32)sin(DEG_TO_RAD(180.0)));
        glMultMatrixf((GLfloat*)&velHMatrix);

        glScalef(p->tumble[0], p->tumble[1], p->tumble[2]);
        glEnable(GL_NORMALIZE);

        return;
    }

    //translate the mesh
    glTranslatef(p->position.x, p->position.y, p->position.z);

    //align mesh along particle's velocity vector
    if (!(p->flags & (PART_PSEUDOBILLBOARD | PART_TRUEBILLBOARD)))
    {
        if (bitTest(meshPart->flags, PART_WORLDSPACE))
        {
            velocity = p->wVel;
            if (!bitTest(meshPart->flags, PART_WORLDSPACEVELOCITY))
            {
                vecSub(velocity, velocity, p->effectVel);       // CAT: the particle keeps the effect velocity (exe [esi+0x98], 0x004dd69a/0x004dd890/0x004ddbd0)
            }
            if (velocity.x == 0.0f && velocity.y == 0.0f && velocity.z == 0.0f)
            {
                velocity.z = 1.0f;
            }
            else
            {
                vecNormalize(&velocity);
            }
        }
        else
        {
            vecScalarMultiply(velocity, p->rvec, p->velR);
            if (velocity.x == 0.0f && velocity.y == 0.0f && velocity.z == 0.0f)
            {
                velocity.z = 1.0f;
            }
            else
            {
                vecNormalize(&velocity);
            }
        }
        matCreateCoordSysFromHeading(&velMatrix, &velocity);
        hmatMakeHMatFromMat(&velHMatrix, &velMatrix);
        glMultMatrixf((GLfloat*)&velHMatrix);
    }

    //tumble the mesh
    if (p->tumble[0] != 0.0f)
    {
        glRotatef(RAD_TO_DEG(p->tumble[0]), 1.0f, 0.0f, 0.0f);
    }
    if (p->tumble[1] != 0.0f)
    {
        glRotatef(RAD_TO_DEG(p->tumble[1]), 0.0f, 1.0f, 0.0f);
    }
    if (p->tumble[2] != 0.0f)
    {
        glRotatef(RAD_TO_DEG(p->tumble[2]), 0.0f, 0.0f, 1.0f);
    }

    //scale the mesh, if applicable
    scale = p->scale / partNLips;
    length = scale;
    if (p->length != 5.0f)
    {
        length = p->length / partNLips;
    }
    if (p->scale != 1.0f)
    {
        glScalef(scale, scale, length);
        if (bRescaleNormal && (scale == length))
        {
            glEnable(GL_RESCALE_NORMAL);
        }
        else
        {
            glEnable(GL_NORMALIZE);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : partRenderMeshSystem
    Description : render a mesh system
    Inputs      : n - number of particles
                  p - pointer to first particle
                  flags - misc flags
    Outputs     :
    Return      : number of live particles; kill the system if 0
----------------------------------------------------------------------------*/
udword partRenderMeshSystem(udword n, particle *p, udword flags, trhandle tex, meshSystem* meshPart)
{
    udword i, hits;
    meshdata* mesh;
    meshdata* mesh2;
    polyentry *polyList;
    materialentry *materialList;
    real32 frac;
    trhandle currentTex = tex;
    bool32 hsColor;
    extern bool8 g_SpecHack;

    glColor3ub(200,200,200);

    hsColor = FALSE;
    if (bitTest(p->flags, PART_XYZSCALE))
    {
        //addColor makes hs effect too distinct
        if (partEffectColor != colBlack)
        {
            hsColor = TRUE;
        }
    }

    rndTextureEnvironment(RTE_Modulate);

    for (i = hits = 0; i < n; i++, p++)
    {
        if (p->lifespan < 0.0f)
        {
            continue;
        }
        else
        {
            hits++;
        }
        if (p->waitspan > 0.0f)
        {
            continue;
        }

        handleIllum(p);

        if (bitTest(p->flags, PART_SPECULAR))
        {
            g_SpecHack = TRUE;
            if (usingShader)
                meshSetSpecular(0, 200,200,200,200);
        }
        else
        {
            g_SpecHack = FALSE;
            if (usingShader)
                meshSetSpecular(-1, 0,0,0,0);
        }

        if (bitTest(p->flags, PART_NODEPTHWRITE))
        {
            glDepthMask(GL_FALSE);
        }
        else
        {
            glDepthMask(GL_TRUE);
        }

        glPushMatrix();

        mesh = p->mesh;
        if ((mesh != NULL) && (mesh != (meshdata*) 0xffffffff) && (mesh != (meshdata*) 0x7fffffff))
        {
            partMeshOrient(p, TRUE, meshPart);

            if (g_SpecHack)
            {
                if (usingShader)
                    shSetExponent(0, p->exponent);
            }

            if (bitTest(flags, PART_ALPHA))
            {
                glColor4f(p->icolor[0], p->icolor[1], p->icolor[2], p->icolor[3]);
                glEnable(GL_BLEND);
                if (usingShader && g_SpecHack)
                {
                    meshSetSpecular(0, RUB(p->icolor[0]), RUB(p->icolor[1]), RUB(p->icolor[2]), RUB(p->icolor[3]));
                }
            }
            else
            {
                glColor3f(p->icolor[0], p->icolor[1], p->icolor[2]);
                if (usingShader && g_SpecHack)
                {
                    meshSetSpecular(0, RUB(p->icolor[0]), RUB(p->icolor[1]), RUB(p->icolor[2]), 255);
                }
            }

            real32 csat[3];

            if ((p->bias[0] + p->bias[1] + p->bias[2]) > 0.0f)
            {
                if (hsColor)
                {
                    p->bias[0] = colReal32(colRed(partEffectColor));
                    p->bias[1] = colReal32(colGreen(partEffectColor));
                    p->bias[2] = colReal32(colBlue(partEffectColor));
                }
                partSaturateAdd(csat, p->bias, p->icolor);

                if (bitTest(flags, PART_ALPHA))
                {
                    glColor4f(csat[0], csat[1], csat[2], p->icolor[3]);
                }
                else
                {
                    glColor3f(csat[0], csat[1], csat[2]);
                }
            }

            //logic to determine what texture to use on this mesh, if any
            if (p->tstruct == NULL && p->tex != TR_Invalid)
            {
                currentTex = p->tex;
            }

#if MESH_VERBOSE_LEVEL
            if (mesh->nPolygonObjects > 1)
            {                                               //display warning if there are more than 1 object in the mesh
                dbgMessagef("There are %d polygon objects in mesh ('%s').", mesh->nPolygonObjects,
#if MESH_RETAIN_FILENAMES
                    mesh->fileName);
#else
                    "N/A");
#endif //MESH_RETAIN_FILENAMES
            }
#endif //MESH_VERBOSE_LEVEL

            if (bitTest(p->flags, PART_PSEUDOBILLBOARD))
            {
                if (bitTest(flags, PART_WORLDSPACE))
                {
                    calcPseudoBillboardMatrixWorld(meshPart->partMat, &meshPart->partPos, p, meshPart);
                }
                else
                {
                    calcPseudoBillboardMatrixNotWorld(meshPart->partMat, &meshPart->partPos, p, meshPart);
                }
            }
            else if (bitTest(p->flags, PART_TRUEBILLBOARD))
            {
                if (bitTest(flags, PART_WORLDSPACE))
                {
                    calcBillboardMatrixWorld(meshPart->partMat, &meshPart->partPos, p);
                }
                else
                {
                    calcBillboardMatrixNotWorld(meshPart->partMat, &meshPart->partPos);
                }
            }

            if (p->mstruct == NULL)
            {                                               //if no morph animation
                if (currentTex != TR_Invalid)
                {                                           //if there is a texture
                    partMeshMaterialPrepare(p, currentTex, &mesh->localMaterial[0], bitTest(flags, PART_ALPHA));
                    rndGLStateLog("meshSystem");
                    meshObjectRenderTex(&mesh->object[0], mesh->localMaterial);
                }
                else
                {                                           //no texture, render solid surfaces
                    rndTextureEnable(FALSE);
                    rndGLStateLog("meshSystem");
                    meshObjectRender(&mesh->object[0], mesh->localMaterial, p->colorScheme);
                }
            }
            else
            {                                               //else there is a morph animation
                mesh2 = partMeshNextMesh(meshPart, p);
              /* it will catch stuff like 0x9cce2641ffffffff on 64-bit... but source of 32/64 should really be found and fixed */
              if ( (0xffffffff == ((memsize)mesh & 0xffffffff)) || (0x7fffffff == ((memsize)mesh & 0x7fffffff))) {
                dbgMessagef("partRenderMeshSystem: Got mesh with suspected flag: %d",mesh);
                return 0; //kill the render system
              }
              if ( (0xffffffff == ((memsize)mesh2 & 0xffffffff)) || (0x7fffffff == ((memsize)mesh2 & 0x7fffffff))) {
                dbgMessagef("partRenderMeshSystem: Got mesh2 with suspected flag: %d",mesh2);
                return 0; //kill the render system
              }
                frac = p->meshFrame - (real32)((sdword)p->meshFrame);

                glDisable(GL_RESCALE_NORMAL);
                glEnable(GL_NORMALIZE);
                polyList = ((meshAnim*)p->mstruct)->mesh->object[0].pPolygonList;
                materialList = ((meshAnim*)p->mstruct)->mesh->localMaterial;
                if (currentTex != TR_Invalid)
                {                                           //if there is a texture
                    partMeshMaterialPrepare(p, currentTex, &materialList[0], bitTest(flags, PART_ALPHA));
                    meshMorphedObjectRenderTex(&mesh->object[0], &mesh2->object[0], polyList, materialList, frac, p->colorScheme);
                }
                else
                {                                           //morphed mesh, mo texture
                    meshMorphedObjectRender(&mesh->object[0], &mesh2->object[0], polyList, materialList, frac, p->colorScheme);
                }
                glDisable(GL_NORMALIZE);
            }

            if (bitTest(p->flags, PART_PSEUDOBILLBOARD))
            {
                undoPseudoBillboardMatrix();
            }
            else if (bitTest(p->flags, PART_TRUEBILLBOARD))
            {
                undoBillboardMatrix();
            }

            if (p->scale != 1.0f)
            {
                glDisable(GL_RESCALE_NORMAL);
            }
        }

        glPopMatrix();
    }

    shSetExponent(0, -1.0f);
    meshSetSpecular(-1, 0, 0, 0, 0);

    g_SpecHack = FALSE;
    glDepthMask(GL_TRUE);

    glDisable(GL_BLEND);

    return hits;
}

/*-----------------------------------------------------------------------------
    Name        : partRenderLineSystem
    Description : render a line system
    Inputs      : n - number of particles
                  p - pointer to first particle
                  flags - misc flags
    Outputs     :
    Return      : number of live particles; kill the system if 0
----------------------------------------------------------------------------*/
udword partRenderLineSystem(udword n, particle *p, udword flags)
{
    udword i, hits;
    vector pos;
    bool32 texEnabled, lightEnabled;
    bool32 alpha = FALSE;

    if (bitTest(flags, PART_ALPHA))
    {
        alpha = TRUE;
        rndAdditiveBlends(FALSE);
        glEnable(GL_BLEND);
    }

    texEnabled = rndTextureEnable(FALSE);
    lightEnabled = rndLightingEnable(FALSE);

    for (i = hits = 0; i < n; i++, p++)
    {
        if (p->lifespan < 0.0f)
            continue;
        else
            hits++;
        if (p->waitspan > 0.0f)
            continue;

        handleIllum(p);

        if (bitTest(p->flags, PART_ADDITIVE))
        {
            rndAdditiveBlends(TRUE);
        }
        else
        {
            rndAdditiveBlends(FALSE);
        }

        glLineWidth(p->scale);
        glBegin(GL_LINES);

        pos = p->position;
        if (bitTest(flags, PART_WORLDSPACE))
        {
            vector rv = p->wVel;
            vecNormalize(&rv);
            if (isnan((double)rv.x) || isnan((double)rv.y) || isnan((double)rv.z) ||
				(ABS(rv.x) < REALlySmall && ABS(rv.y) <= REALlySmall && ABS(rv.z) <= REALlySmall))
            {
                rv.z = 1.0f;
            }
            if (!isnan((double)p->length))
            {
                vecMultiplyByScalar(rv, p->length);
            }
            vecAddTo(pos, rv);
        }
        else
        {
            vector rv = p->rvec;
            vecMultiplyByScalar(rv, p->velR);
            rv.z += p->velLOF;
            if (rv.x == 0.0f && rv.y == 0.0f && rv.z == 0.0f)
            {
                rv.z = 1.0f;
            }
            vecNormalize(&rv);
            vecMultiplyByScalar(rv, p->length);
            vecAddTo(pos, rv);
        }

        if (alpha)
        {
            glColor4f(p->icolor[0], p->icolor[1], p->icolor[2], p->icolor[3]);
            glVertex3f(p->position.x, p->position.y, p->position.z);
            glColor4f(p->icolor[0], p->icolor[1], p->icolor[2], p->icolor[3]);
            glVertex3f(pos.x, pos.y, pos.z);
        }
        else
        {
            glColor3f(p->icolor[0], p->icolor[1], p->icolor[2]);
            glVertex3f(p->position.x, p->position.y, p->position.z);
            glColor3f(p->icolor[0], p->icolor[1], p->icolor[2]);
            glVertex3f(pos.x, pos.y, pos.z);
        }

        glEnd();
    }

    if (alpha)
    {
        glDisable(GL_BLEND);
    }
    rndAdditiveBlends(FALSE);

    rndTextureEnable(texEnabled);
    rndLightingEnable(lightEnabled);

    return hits;
}

/*-----------------------------------------------------------------------------
    Name        : partRenderCubeSystem
    Description : render a cube system
    Inputs      : n - number of particles
                  p - pointer to first particle
                  flags - misc flags
    Outputs     :
    Return      : number of live particles; kill the system if 0
----------------------------------------------------------------------------*/
udword partRenderCubeSystem(udword n, particle *p, udword flags)
{
    udword i, hits;
    bool32 alpha = FALSE;

    if (bitTest(flags, PART_ALPHA))
    {
        alpha = TRUE;
        rndAdditiveBlends(FALSE);
        glEnable(GL_BLEND);
    }

    for (i = hits = 0; i < n; i++, p++)
    {
        if (p->lifespan < 0.0f)
            continue;
        else
            hits++;
        if (p->waitspan > 0.0f)
            continue;
        glPushMatrix();
        glTranslatef(p->position.x, p->position.y, p->position.z);
        handleIllum(p);
        drawBox(p->scale * 0.5f, GL_TRIANGLE_STRIP, p, alpha);
        glPopMatrix();
    }

    if (alpha)
        glDisable(GL_BLEND);

    return hits;
}

/*-----------------------------------------------------------------------------
    Name        : partRenderPointSystem
    Description : render a point system
    Inputs      : n - number of particles
                  p - pointer to first particle
                  flags - misc flags
    Outputs     :
    Return      : number of live particles; kill the system if 0
----------------------------------------------------------------------------*/
udword partRenderPointSystem(udword n, particle *p, udword flags)
{
    udword i, hits;
    bool32 alpha = FALSE;

    bool32 texEnabled, lightEnabled;

    texEnabled = rndTextureEnable(FALSE);
    lightEnabled = rndLightingEnable(FALSE);

    if (bitTest(flags, PART_ALPHA))
    {
        alpha = TRUE;
        glEnable(GL_BLEND);
        if (bitTest(p->flags, PART_ADDITIVE))
        {
            rndAdditiveBlends(TRUE);
        }
        else
        {
            rndAdditiveBlends(FALSE);
        }
    }

    for (i = hits = 0; i < n; i++, p++)
    {
        if (p->lifespan < 0.0f)
            continue;
        else
            hits++;
        if (p->waitspan > 0.0f)
            continue;

        handleIllum(p);

        glPointSize((p->scale <= 3.0f) ? p->scale : 3.0f);
        glBegin(GL_POINTS);
        if (alpha)
            glColor4f(p->icolor[0], p->icolor[1], p->icolor[2], p->icolor[3]);
        else
            glColor3f(p->icolor[0], p->icolor[1], p->icolor[2]);
        glVertex3f(p->position.x, p->position.y, p->position.z);
        glEnd();
    }

    if (alpha)
    {
        glDisable(GL_BLEND);
    }

    rndTextureEnable(texEnabled);
    rndLightingEnable(lightEnabled);

    rndAdditiveBlends(FALSE);
    return hits;
}

/*-----------------------------------------------------------------------------
    Name        : partRenderSystem
    Description : render a particle system.  passes control to other more
                  specific functions.  assumes MODELVIEW already set.
    Inputs      : psys - the particle system
    Outputs     : sets psys->isAlive to FALSE if no particle is left alive
    Return      :
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        : cat_partRenderEBoltSystem
    Description : CAT: new (cat_partRenderUnk1System 0x004de880). Renders an electric-bolt system: each particle is
                  a line strip of p->segments segments along its direction (wVel for worldspace systems, rvec * velR
                  + velLOF otherwise), each segment p->length / p->segments long; every inner vertex is moved by a
                  random offset in [-a, a] per axis, a = |turbulance * step| / 2, drawn from RNG stream 0 (the exe's
                  ranRandomFn(0) at 0x004decf6). Line width is p->scale.
    Inputs      : n - number of particles
                  p - pointer to first particle
                  flags - system flags
    Outputs     :
    Return      : number of live particles; kill the system if 0
----------------------------------------------------------------------------*/
udword cat_partRenderEBoltSystem(udword n, particle *p, udword flags)
{
    udword i, hits = 0;
    sdword j;
    bool32 alpha, texEnabled, lightEnabled, additive;
    vector step, pos, vertex, t;
    real32 amplitude, low, scale;

    glPushAttrib(GL_LINE_BIT);
    alpha = bitTest(flags, PART_ALPHA) ? TRUE : FALSE;
    texEnabled = rndTextureEnable(FALSE);
    lightEnabled = rndLightingEnable(FALSE);
    additive = rndAdditiveBlends(p->flags & PART_ADDITIVE);    //the first particle's blend mode for all

    for (i = 0; i < n; i++, p++)
    {
        if (p->lifespan < 0.0f)
        {
            continue;
        }
        hits++;
        if (p->waitspan > 0.0f)
        {
            continue;
        }
        //direction of the bolt
        if (bitTest(flags, PART_WORLDSPACE))
        {
            step = p->wVel;
            vecNormalize(&step);
            if (isnan((double)step.x) || isnan((double)step.y) || isnan((double)step.z) ||
                (ABS(step.x) < REALlySmall && ABS(step.y) <= REALlySmall && ABS(step.z) <= REALlySmall))
            {
                step.z = 1.0f;
            }
        }
        else
        {
            step = p->rvec;
            vecMultiplyByScalar(step, p->velR);
            step.z += p->velLOF;
            if (isnan((double)step.x) || isnan((double)step.y) || isnan((double)step.z) ||
                (ABS(step.x) < REALlySmall && ABS(step.y) <= REALlySmall && ABS(step.z) <= REALlySmall))
            {
                step.z = 1.0f;
            }
            vecNormalize(&step);
        }
        if (!isnan((double)(p->length / (real32)p->segments)))
        {                                                   //one segment's worth of the bolt
            vecMultiplyByScalar(step, p->length / (real32)p->segments);
        }
        //jitter amplitude
        t.x = p->turbulance * step.x;
        t.y = p->turbulance * step.y;
        t.z = p->turbulance * step.z;
        amplitude = fsqrt(t.x * t.x + t.y * t.y + t.z * t.z) * 0.5f;

        glBegin(GL_LINE_STRIP);
        glLineWidth(p->scale);
        if (alpha)
        {
            glColor4f(p->icolor[0], p->icolor[1], p->icolor[2], p->icolor[3]);
        }
        else
        {
            glColor3f(p->icolor[0], p->icolor[1], p->icolor[2]);
        }
        pos = p->position;
        glVertex3f(pos.x, pos.y, pos.z);
        for (j = 0; j < (sdword)p->segments; j++)
        {
            vecAddTo(pos, step);
            vertex = pos;
            if (j < (sdword)p->segments - 1)
            {                                               //every vertex but the end point jitters
                low = -amplitude;
                scale = (amplitude - low) * (1.0f / 4294967296.0f);
                vertex.x = (real32)((long double)ranRandom(RANDOM_STREAM_0) * scale + low + pos.x);
                vertex.y = (real32)((long double)ranRandom(RANDOM_STREAM_0) * scale + low + pos.y);
                vertex.z = (real32)((long double)ranRandom(RANDOM_STREAM_0) * scale + low + pos.z);
            }
            glVertex3f(vertex.x, vertex.y, vertex.z);
        }
        glEnd();
    }

    rndLightingEnable(lightEnabled);
    rndTextureEnable(texEnabled);
    rndAdditiveBlends(additive);
    glPopAttrib();
    return hits;
}

void partRenderSystem(psysPtr psys)
{
    pointSystem *pp;
    particle *p = NULL;
    udword hits;
    sdword wasLit;
    bool32 isWorldspace;
    vector position;

    billSystem* billPart;

    wasLit = rndLightingEnable(FALSE);

    pp = (pointSystem*)psys;
    storeIllum();
    isWorldspace = bitTest(pp->flags, PART_WORLDSPACE) ? TRUE : FALSE;

    p = (particle*)(psys + partHeaderSize(psys));
    if (p->lit)
    {
        rndLightingEnable(TRUE);
    }

    switch (pp->t)
    {
    case PART_BILLBOARD:
        billPart = (billSystem*)pp;
        memcpy(&position, &billPart->position, sizeof(vector));
        if (!isWorldspace)
        {
            //will push matrix
            partPerformHacking(((billSystem*)pp)->partMat, position);
        }
        hits = partRenderBillSystem(pp->n, p, pp->flags, position,
                                    billPart->uv, billPart, isWorldspace);
        if (!isWorldspace)
        {
            //pop the matrix
            glPopMatrix();
        }
        break;

    case PART_MESH:
        alodEnable(FALSE);
        hits = partRenderMeshSystem(pp->n, p, pp->flags, ((meshSystem*)pp)->tex,
                                    (meshSystem*)pp);
        alodEnable(TRUE);
        break;

    case PART_LINES:
        hits = partRenderLineSystem(pp->n, p, pp->flags);
        break;

    case PART_CUBES:
        hits = partRenderCubeSystem(pp->n, p, pp->flags);
        break;

    case PART_POINTS:
        hits = partRenderPointSystem(pp->n, p, pp->flags);
        break;

    case PART_EBOLTS:                                       // CAT: partRenderSystem 0x004df3b2
        hits = cat_partRenderEBoltSystem(pp->n, p, pp->flags);
        break;

    case PART_LENSFLARES:
        // CAT: nothing is drawn here (the lens flare renderer draws the sources); the first particle's source is
        //      marked (state 0) and the system stays alive (0x004df3c5)
        if (p->lensFlare != NULL)
        {
            p->lensFlare->state = 0;
        }
        hits = 1;
        break;

    default:
#if PART_Verbose > 0
        dbgMessagef("unknown particle system type: %d", pp->t);
#endif
        return;
    }

    restoreIllum();

    if (!hits)                      //kill the system
    {
        pp->isAlive = FALSE;
    }
    rndLightingEnable(wasLit);
    rndAdditiveBlends(FALSE);
}

//constant delta
real32 partRealDelta(real32 n, real32 d)
{
    return n + d;
}

//randomized delta
real32 partRealDist(real32 n, real32 d)
{
    real32 r = (real32)((real64)(ranRandom(RANDOM_PARTICLE_STREAM) % 1000000) / 1000000.0);
    real32 sign = (ranRandom(RANDOM_PARTICLE_STREAM) % 2 == 0) ? -1.0f : 1.0f;
    if (d < 0.0f)
        sign = -1.0f;
//    if (n + sign*r*d < 0) dbgWarningf(DBG_Loc, "/mn/ partRealDist input n=%f, d=%f; derived r=%f, sign=%f; return n+sign*r*d=%f", n, d, r, sign, n + sign*r*d);
    return n + sign*r*d;
}

//constant delta, will handle alpha
void partColorDelta(real32 *c, real32 *d, udword flags, real32 dt)
{
    real32 cr, cg, cb, ca;
    if (d[0] != 0.0f)
    {
        cr = partRealDelta(c[0], d[0] * dt);
        RealClamp(cr);
        c[0] = cr;
    }
    if (d[1] != 0.0f)
    {
        cg = partRealDelta(c[1], d[1] * dt);
        RealClamp(cg);
        c[1] = cg;
    }
    if (d[2] != 0.0f)
    {
        cb = partRealDelta(c[2], d[2] * dt);
        RealClamp(cb);
        c[2] = cb;
    }
    if (bitTest(flags, PART_ALPHA) && d[3] != 0.0f)
    {
        ca = partRealDelta(c[3], d[3] * dt);
        RealClamp(ca);
        c[3] = ca;
    }
}

//randomized color delta
void partColorDist(real32 *out, real32 *c, real32 dr, real32 dg, real32 db)
{
    real32 cr, cg, cb;
    cr = partRealDist(c[0], dr);
    cg = partRealDist(c[1], dg);
    cb = partRealDist(c[2], db);
    RealClampNeg(cr);
    RealClampNeg(cg);
    RealClampNeg(cb);
    out[0] = cr;
    out[1] = cg;
    out[2] = cb;
}

//like partColorDist, but with alpha
void partColorDistA(real32 *out, real32 *c, real32 dr, real32 dg, real32 db, real32 da)
{
    real32 ca;
    partColorDist(out, c, dr, dg, db);
    ca = c[3];
    ca = partRealDist(ca, da);
    RealClampNeg(ca);
    out[3] = ca;
}

real32 fracPart(real32 f)
{
    sdword i = (sdword)f;
    return (real32)fabs((real64)((real32)i - f));
}

#if PART_TEST_MORPH
meshAnim* partSetupTestMeshMorphBlock()
{
    meshAnim* ma;
    meshAnim* a;

    ma = (meshAnim*)memAlloc(5*sizeof(meshAnim), "mesh animation block", 0);

#ifdef _WIN32
#define TMP_MESH_PATH "etg\\meshes\\"
#else
#define TMP_MESH_PATH "etg/meshes/"
#endif

    a = ma;
    a->mesh = meshLoad(TMP_MESH_PATH "head00.geo");
    a->flags = PART_LOOPSTART;

    a++;
    a->mesh = meshLoad(TMP_MESH_PATH "head01.geo");
    a->flags = 0;

    a++;
    a->mesh = meshLoad(TMP_MESH_PATH "head02.geo");
    a->flags = 0;

    a++;
    a->mesh = meshLoad(TMP_MESH_PATH "head03.geo");
    a->flags = PART_LOOP;

    a++;
    a->mesh = NULL;
    a->flags = 0;

#undef TMP_MESH_PATH

    return ma;
}
#endif

/*-----------------------------------------------------------------------------
    Name        : partMeshNextMesh
    Description : mesh animations track the CURRENT frame of animation; this fn
                  returns the NEXT frame of animation for lerping
    Inputs      : psys - particle system header
                  p - the particular particle
    Outputs     :
    Return      : mesh object of the next animation frame
----------------------------------------------------------------------------*/
meshdata* partMeshNextMesh(meshSystem* psys, particle* p)
{
    sdword    frameNo   = partAdvanceMeshMorph(psys, p);
    meshAnim* animblock = (meshAnim*)p->mstruct;
    meshAnim* frame     = animblock + frameNo;
    
    return frame->mesh;
}

/*-----------------------------------------------------------------------------
    Name        : partAdvanceMeshMorph
    Description : actually updates the frame of a mesh animation.
                  p->meshFrame is ASSUMED VALID
    Inputs      : p - the partice to update
    Outputs     :
    Return      : the frame of animation is returned
----------------------------------------------------------------------------*/
//this fn assumes that the current frame is valid (ie. not an end or overflow)
sdword partAdvanceMeshMorph(meshSystem* psys, particle* p)
{
    meshAnim* animblock;
    meshAnim* prev;
    meshAnim* next;
    sdword    frame;

    animblock = (meshAnim*)p->mstruct;
    prev = animblock + (sdword)p->meshFrame;     //floor for previous

    //-- seek to next position --

    //maybe we're a loopend
    if (prev->flags == PART_LOOP)
    {
        if (p->loopCount > 0)
        {
            p->loopCount--;
        }
        else if (p->loopCount == 0)
        {
            goto NO_LOOP;
        }
        //seek to loopstart
        for (next = animblock, frame = 0;; frame++, next++)
        {
            if (next->flags == PART_LOOPSTART)
            {
                return frame;   //found the start of the loop
            }
            else if (next->mesh == NULL)
            {
                return -1;      //error condition
            }
        }
    }

 NO_LOOP:

    //find the trivial next
    frame = (sdword)p->meshFrame + 1;
    next = animblock + frame;
    if (next->mesh == NULL || next->mesh == (meshdata*) 0xffffffff || next->mesh == (meshdata*) 0x7fffffff)
    {
        if (p->loopCount == 0)
        {
            //we've exceeded our loopcount, so don't wrap around
            frame = (sdword)p->meshFrame;
        }
        else
        {
            next = animblock;       //wrapped around
            frame = 0;
        }
    }

    return frame;
}

/*-----------------------------------------------------------------------------
    Name        : partSetMeshFromAnimation
    Description : sets the mesh on a particle from the current frame of animation.
                  p->meshFrame is ASSUMED VALID
    Inputs      : p - the particle
    Outputs     : p->mesh is updated
    Return      :
----------------------------------------------------------------------------*/
void partSetMeshFromAnimation(particle* p)
{
    meshAnim* animblock;
    meshAnim* frame;

    animblock = (meshAnim*)p->mstruct;
    frame = animblock + (sdword)p->meshFrame;
    p->mesh = frame->mesh;
}

/*-----------------------------------------------------------------------------
    Name        : partUpdateMeshAnimation
    Description : updates the frame of a mesh animation
    Inputs      : psys - mesh particle system header
                  part - the particle to update
                  dt - elapsed time
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
//when rendering, use floor(meshFrame) as "from", and the result of
//partAdvanceMeshMorph(..) as "to"
void partUpdateMeshAnimation(meshSystem* psys, particle* part, real32 dt)
{
    real32 prevFrame, nextFrame, frac;
    sdword iFrames, frame, i;

    if (part->meshRate == 0.0f)         //nothing to update
    {
        return;
    }

    prevFrame = part->meshFrame;
    nextFrame = prevFrame + dt * part->meshRate;

    if ((sdword)nextFrame == (sdword)prevFrame)
    {
        //no animation advance, just new lerp parameters
        part->meshFrame = nextFrame;
        partSetMeshFromAnimation(part);
        return;
    }

    iFrames = (sdword)nextFrame - (sdword)prevFrame;
    frac = nextFrame - (real32)floor((real64)nextFrame);
    //seek iFrames into the animation
    for (i = 0; i < iFrames; i++)
    {
        frame = partAdvanceMeshMorph(psys, part);
        part->meshFrame = (real32)frame + frac;
    }

    partSetMeshFromAnimation(part);
}

/*
 * returns true if this texture handle is last/invalid (teminator tex)
 */
int is_final_tex(trhandle tex)
{
    if (tex == 0x7fffffff) return 1;

#ifdef HW_PTR_64_FIX_ME
    if (tex == 0xffffffff) {
      return 1;
    }
    if (0x7fffffff == (tex & 0x7fffffff)) { 
#ifdef HW_BUILD_FOR_DEBUGGING
      /* it will catch stuff like 0x9cce2641ffffffff on 64-bit... but source of 32/64 should really be found and fixed */
      dbgWarningf(__FILE__,__LINE__, "is_final_tex got invalid tex pointer 0x%lx - trying to work around", tex);
#endif
      return 1;
    }
    if (tex >= TR_RegistrySize) { dbgFatalf(DBG_Loc, "tex handle 0x%lx is broken in is_final_tex, unable to continue", tex); }
#endif
        
    return 0;
}


/*-----------------------------------------------------------------------------
    Name        : partUpdateTexAnim
    Description : returns an incremented frame counter for a texture animation
                  (frame++)
    Inputs      : bsys - billboard particle system header
                  p - the particular particle we're interested in
    Outputs     :
    Return      : incremented frame counter
----------------------------------------------------------------------------*/
sdword partAdvanceTexAnim(billSystem* bsys, particle* p)
{
    texAnim* animblock;
    texAnim* prev;
    texAnim* next;
    sdword   frame;

    animblock = (texAnim*)p->tstruct;
    prev = animblock + (sdword)p->currentFrame;     //floor for previous

    //-- seek to next position --

    //maybe we're a loopend
    if (p->loop && prev->flags == PART_LOOP)
    {
        //seek to loopstart
        for (next = animblock, frame = 0;; frame++, next++)
        {
            if (next->flags == PART_LOOPSTART)
            {
                return frame;   //found the start of the loop
            }
            else if (is_final_tex(next->tex))
            {
                return -1;      //error condition
            }
        }
    }

    //find the trivial next
    frame = (sdword)p->currentFrame + 1;
    next = animblock + frame;
    if (is_final_tex(next->tex))
    {
        if (!p->loop)
        {
            //we've exceeded our loopcount, so don't wrap around
            frame = (sdword)p->currentFrame;
        }
        else
        {
            next = animblock;       //wrapped around
            frame = 0;
        }
    }

    return frame;
}

/*-----------------------------------------------------------------------------
    Name        : partSetTexFromAnimation
    Description : sets the texture on a particle from the current frame of animation.
                  part->currentFrame is ASSUMED VALID
    Inputs      : bsys - system header
                  part - individual particle
    Outputs     : system's texture is modified
    Return      :
----------------------------------------------------------------------------*/
void partSetTexFromAnimation(psysPtr bsys, particle* part)
{
    texAnim* animblock;
    texAnim* frame;
    memsize args[6];

    animblock = (texAnim*)part->tstruct;
    frame = animblock + (sdword)part->currentFrame;
    // CAT: partModifyTexture takes the ETG argument array (partUpdateSystem 0x004dfbf9 builds it on its stack)
    args[0] = (memsize)bsys;
    args[1] = (memsize)frame->tex;
    args[2] = (memsize)frame->uv[0][0];
    args[3] = (memsize)frame->uv[0][1];
    args[4] = (memsize)frame->uv[1][0];
    args[5] = (memsize)frame->uv[1][1];
    partModifyTexture(args);
}

/*-----------------------------------------------------------------------------
    Name        : partUpdateAnimation
    Description : updates the frame of a texture animation
    Inputs      : bsys - system header
                  part - particular particle
                  dt - time elapsed
    Outputs     : system's texture is modified, particle's frame counter is updated
    Return      :
----------------------------------------------------------------------------*/
void partUpdateAnimation(billSystem* bsys, particle* part, real32 dt)
{
    real32 prevFrame, nextFrame, frac;
    sdword iFrames, frame, i;

    if (part->rate == 0.0f)
    {
        return;
    }

    prevFrame = part->currentFrame;
    nextFrame = prevFrame + dt * part->rate;

    if ((sdword)nextFrame == (sdword)prevFrame)
    {
        part->currentFrame = nextFrame;
        partSetTexFromAnimation((psysPtr)bsys, part);
        return;
    }

    iFrames = (sdword)nextFrame - (sdword)prevFrame;
    frac = nextFrame - (real32)floor((real64)nextFrame);
    for (i = 0; i < iFrames; i++)
    {
        frame = partAdvanceTexAnim(bsys, part);
        part->currentFrame = (real32)frame + frac;
    }

    partSetTexFromAnimation((psysPtr)bsys, part);
}

/*-----------------------------------------------------------------------------
    Name        : partUpdateSystem
    Description : ticks a particle system by given elapsed time
    Inputs      : psys - particle system pointer
                  dt - elapsed time since last call
                  velvec - velocity vector to be added to velLOF
                           and velR
    Outputs     :
    Return      : TRUE if system is dead, FALSE otherwise
----------------------------------------------------------------------------*/
bool8 partUpdateSystem(psysPtr psys, real32 dt, vector* velvec)
{
    billSystem *pp;
    particle *p = NULL;
    udword n, i, hits;
    vector rvec;
    bool32 draggin;

    pp = (billSystem*)psys;      //default assumption
    n = pp->n;
    if (pp->angDelta != 0.0f)
    {
        pp->ang += dt * pp->angDelta;
    }

    p = (particle*)(psys + partHeaderSize(psys));

    if (p == NULL)
    {
        dbgMessage("!! invalid particle system !!");
        return TRUE;
    }

    draggin = (pp->drag != 1.0f);

    for (i = hits = 0; i < n; i++, p++)
    {
        //mesh tumble specifics
        if (pp->t == PART_MESH)
        {
            p->tumble[0] += dt * p->deltaTumble[0];
            p->tumble[1] += dt * p->deltaTumble[1];
            p->tumble[2] += dt * p->deltaTumble[2];
        }

        if (p->waitspan > 0.0f)
        {
            p->waitspan -= dt;
            if (p->waitspan < 0.0f)
            {
                p->waitspan = 0.0f;
            }
            hits++;
            continue;       //don't dec lifespan if waiting
        }

        //sprite/mesh system specifics
        if (p->tstruct != NULL)
        {
            partUpdateAnimation(pp, p, dt);
        }
        if (p->mstruct != NULL)
        {
            partUpdateMeshAnimation((meshSystem*)pp, p, dt);
        }
        if (p->deltaRot != 0.0f)
        {
            p->rot += dt * p->deltaRot;
        }

        //lines
        if (p->deltaLength)
        {
            p->length += dt * p->deltaLength;
            if (isnan((double)p->length))
            {
                p->length = 1.0f;
            }
            // CAT: a clamped length stops at zero (partUpdateSystem 0x004dfdb5)
            if (p->lengthClamp && p->length < 0.0f)
            {
                p->length = 0.0f;
            }
        }

        //kinematics
        if (bitTest(pp->flags, PART_WORLDSPACE))
        {
            vector adjWVel, adjWAccel;

            //don't update position if XYZ scaling (hyperspace effect)
            if (!bitTest(p->flags, PART_XYZSCALE))
            {
                //setup
                adjWVel = p->wVel;
                adjWAccel = p->wAccel;
                vecMultiplyByScalar(adjWVel, dt);
                vecMultiplyByScalar(adjWAccel, dt);
                //velocity
                vecAddTo(adjWVel, adjWAccel);
                //update
                vecAddTo(p->position, adjWVel);
                vecAddTo(p->wVel, adjWAccel);
                //drag
                vecMultiplyByScalar(p->wVel, pp->drag);
                vecMultiplyByScalar(p->wAccel, pp->drag);
            }
        }
        else
        {
            //position
            p->position.z += dt * p->velLOF;
            rvec = p->rvec;
            vecMultiplyByScalar(rvec, dt * p->velR);
            vecAddTo(p->position, rvec);

            //velocity
            p->velLOF += dt * p->deltaVelLOF;
            p->velR += dt * p->deltaVelR;
            if (draggin)
            {
                p->velLOF *= pp->drag;
                p->velR *= pp->drag;
            }
        }

        // CAT: a lens flare source follows its particle (0x004dffaa)
        if (p->lensFlare != NULL)
        {
            p->lensFlare->position = p->position;
        }

        //appearance
        if (p->deltaScale != 0.0f)
        {                                                   //scale the particle
            p->scale += dt * p->deltaScale;
            if (p->scale < 0.0f)
            {                                               //clamp at zero
                p->scale = 0.0f;
            }
            // CAT: the Y scale grows the same way (0x004dfff1)
            p->scaleY += dt * p->deltaScale;
            if (p->scaleY < 0.0f)
            {
                p->scaleY = 0.0f;
            }
        }

        partColorDelta(p->icolor, p->deltaColor, pp->flags, dt);

        //FIXME: does this make sense?
        if (p->icolor[3] < 1.0f)
        {
            pointSystem* sys = (pointSystem*)psys;
            bitSet(sys->flags, PART_ALPHA);
        }

        if (p->lit)
        {
            p->illum += dt * p->deltaIllum;
            RealClamp(p->illum);
        }

        //lifespan
        p->lifespan -= dt;
        if (p->lifespan > 0.0f)
        {
            hits++;
        }
    }

    return((bool8)((hits == 0) ? TRUE : FALSE));
}

bool32 g_Billboarded;

real64 randomAngle(udword a)
{
    udword r = ranRandom(RANDOM_PARTICLE_STREAM) % a;
    return DEG_TO_RAD((real64)r);
}

//make a spherically distributed particle
vector partMakeSPos(real32 offsetLOF, real32 offsetR, real32 offsetTheta,
                    real32 deltaLOF, real32 deltaLOFDist,
                    real32 deltaR, real32 deltaRDist,
                    real32 deltaTheta, real32 deltaThetaDist,
                    vector *rVec)
{
    vector xyz;
    hvector hvec, rvec;
    real32 sinTheta, cosTheta;
    hmatrix result;

    //deltas
    deltaLOF = partRealDist(deltaLOF, deltaLOFDist);
    deltaR = partRealDist(deltaR, deltaRDist);
    deltaTheta = partRealDist(deltaTheta, deltaThetaDist);

    //offsets
    offsetLOF = partRealDelta(offsetLOF, deltaLOF);
    offsetR = partRealDelta(offsetR, deltaR);
    offsetTheta = partRealDelta(offsetTheta, deltaTheta);

    //cartesian
    {
        real64 a = randomAngle(360);
        sinTheta = (real32)sin(a);
        cosTheta = (real32)cos(a);
    }
    vecZeroVector(xyz);
    xyz.x = 1.0f;
    vecMakeHVecFromVec(hvec, xyz);
    hmatMakeRotAboutZ(&result, sinTheta, cosTheta);
    hmatMultiplyHMatByHVec(&rvec, &result, &hvec);
    {
        real64 a = randomAngle(360);
        sinTheta = (real32)sin(a);
        cosTheta = (real32)cos(a);
    }
    hmatMakeRotAboutX(&result, sinTheta, cosTheta);
    hmatMultiplyHMatByHVec(&hvec, &result, &rvec);
    rvec = hvec;
    hmatMultiplyHMatByHVec(&rvec, &result, &hvec);
    vecHomogenize(&xyz, &rvec);
    *rVec = xyz;
    vecNormalize(rVec);
    vecMultiplyByScalar(xyz, offsetR);
    xyz.z += offsetLOF;

    //done
    return xyz;
}

/*-----------------------------------------------------------------------------
    Name        : partMakePosition
    Description : turns a many-variabled cylindrical coordinate system
                  interpretation into something nice.
    Inputs      : offsetLOF - distance along line of fire
                  offsetR - radius component
                  offsetTheta - theta component
                  deltaLOF - variance in offsetLOF
                  deltaLOFDist - variance in deltaLOF
                  deltaR - variance in offsetR
                  deltaRDist - variance in deltaR
                  deltaTheta - variance in theta
                  deltaThetaDist - variance in deltaTheta
                  rVec - pointer to a vector to recieve radial velocity
    Outputs     : rVec - unitized offsetR rotated by theta
    Return      : a 3d vector (x,y,z)
----------------------------------------------------------------------------*/
vector partMakePosition(real32 offsetLOF, real32 offsetR, real32 offsetTheta,
                        real32 deltaLOF, real32 deltaLOFDist,
                        real32 deltaR, real32 deltaRDist,
                        real32 deltaTheta, real32 deltaThetaDist,
                        vector *rVec)
{
    vector xyz;
    vector hvec;
    real32 sinTheta, cosTheta;
    matrix result;

    //deltas
    deltaLOF = partRealDist(deltaLOF, deltaLOFDist);
    deltaR = partRealDist(deltaR, deltaRDist);
    deltaTheta = partRealDist(deltaTheta, deltaThetaDist);

    //offsets
    offsetLOF = partRealDelta(offsetLOF, deltaLOF);
    offsetR = partRealDelta(offsetR, deltaR);
    offsetTheta = partRealDelta(offsetTheta, deltaTheta);

    //cartesian
    sinTheta = (real32)sin((real64)offsetTheta);
    cosTheta = (real32)cos((real64)offsetTheta);
    vecZeroVector(xyz);
    xyz.x = offsetR;
    hvec = xyz;
    matMakeRotAboutZ(&result, sinTheta, cosTheta);
    matMultiplyMatByVec(&xyz, &result, &hvec);
    hvec.x = 1.0f;
    hvec.y = hvec.z = 0.0f;
    matMultiplyMatByVec(rVec, &result, &hvec);

    xyz.z += offsetLOF;

    //done
    return xyz;
}

/*-----------------------------------------------------------------------------
    Name        : partFillGenericParticles
    Description : fills up the generic portion of a particle structure with the
                  current particle state (particleAttribs)
    Inputs      : n - number of particles
                  psys - the particle system, type-specific header + generic portion
                  headerlength - length of the type-specific portion
                  dist - PART_CYLINDER or PART_SPHERE
    Outputs     : fills up the generic portion of psys appropriately
    Return      :
----------------------------------------------------------------------------*/
void partFillGenericParticles(udword n, psysPtr psys,
                              udword headerlength, udword dist,
                              bool32 isWorldspace)
{
    udword i, pos, size;
    vector rvec;
    particle *p;
    bool32 alpha = FALSE;

    if (bitTest(pat.flags, PART_ALPHA))
        alpha = TRUE;

    for (i = 0; i < n; i++)
    {
        //call the particle creation callback, if any
        if (partCreateCallback)
        {
            partCreateCallback(partCreateUserValue, partCreateUserData);
        }
        size = sizeof(particle);
        pos = headerlength + i*size;
        p = (particle*)(psys + pos);

        //flags
        p->flags = 0;
        if (pat.specularOnly)
        {
            bitSet(p->flags, PART_SPECULAR);
        }
        if (pat.stippleAlpha)
        {
            bitSet(p->flags, PART_STIPPLE);
        }
        if (pat.additiveBlends)
        {
            bitSet(p->flags, PART_ADDITIVE);
        }
        if (pat.pseudoBillboard)
        {
            bitSet(p->flags, PART_PSEUDOBILLBOARD);
        }
        if (pat.trueBillboard)
        {
            bitSet(p->flags, PART_TRUEBILLBOARD);
        }
        if (pat.noDepthWrite)
        {
            bitSet(p->flags, PART_NODEPTHWRITE);
        }

        //sprite system specifics
        p->tex = TR_Invalid;
        p->rot = partRealDist(pat.ang, pat.angDist);
        p->deltaRot = partRealDist(pat.angDelta, pat.angDeltaDist);

        p->tstruct = pat.tstruct;
        p->rate = pat.rate;
        p->meshRate = pat.meshRate;
        p->loop = pat.loop;
        p->currentFrame = (real32)pat.startFrame;

        //position
        if (dist == PART_CYLINDER)
        {
            p->position = partMakePosition(pat.offsetLOF, pat.offsetR, pat.offsetTheta,
                                           pat.deltaLOF, pat.deltaLOFDist,
                                           pat.deltaR[0], pat.deltaRDist[0],
                                           pat.deltaR[1], pat.deltaRDist[1],
                                           &rvec);
        }
        else
        {
            p->position = partMakeSPos(pat.offsetLOF, pat.offsetR, pat.offsetTheta,
                                       pat.deltaLOF, pat.deltaLOFDist,
                                       pat.deltaR[0], pat.deltaRDist[0],
                                       pat.deltaR[1], pat.deltaRDist[1],
                                       &rvec);
        }
        if (pat.offsetArray != NULL)
        {
            vecAddTo(p->position, pat.offsetArray[i]);
            //comment out the previous line and uncomment this section to get a mapping from
            //Lightwave ship coords to Homeworld ship coords
            /*
            p->position.x += pat.offsetArray[i].y;
            p->position.y += pat.offsetArray[i].z;
            p->position.z -= pat.offsetArray[i].x;
            */

        }

        p->rvec = rvec;
        vecNormalize(&p->rvec);
        //velocity
        p->deltaVelLOF = partRealDist(pat.deltaVelLOF, pat.deltaVelLOFDist);
        p->deltaVelR = partRealDist(pat.deltaVelR, pat.deltaVelRDist);
        p->velLOF = partRealDist(pat.velLOF, pat.velLOFDist);
        p->velR = partRealDist(pat.velR, pat.velRDist);
        //appearance
        p->scale = partRealDist(pat.scale, pat.scaleDist);
        p->scaleY = p->scale;                               // CAT: both scales start equal (0x004e0cea fst/fstp +0x50/+0x54)
        p->deltaScale = partRealDist(pat.deltaScale, pat.deltaScaleDist);
        if (alpha)
        {
            partColorDistA(p->icolor, pat.icolor, pat.colorDist[0], pat.colorDist[1],
                           pat.colorDist[2], pat.colorDist[3]);
            partColorDistA(p->deltaColor, pat.deltaColor, pat.deltaColorDist[0],
                           pat.deltaColorDist[1], pat.deltaColorDist[2],
                           pat.deltaColorDist[3]);
        }
        else
        {
            partColorDist(p->icolor, pat.icolor, pat.colorDist[0],
                          pat.colorDist[1], pat.colorDist[2]);
            partColorDist(p->deltaColor, pat.deltaColor, pat.deltaColorDist[0],
                          pat.deltaColorDist[1], pat.deltaColorDist[2]);
            p->icolor[3] = 1.0f;
        }
        p->lit = pat.lit;
        p->illum = partRealDist(pat.illum, pat.illumDist);
        p->deltaIllum = partRealDist(pat.deltaIllum, pat.deltaIllumDist);
        p->exponent = partRealDist(pat.exponent, pat.exponentDist);
        p->colorScheme = pat.colorScheme;
        //lifespan
        p->lifespan = partRealDist(pat.lifespan, pat.lifespanDist);
        p->waitspan = partRealDist(pat.waitspan, pat.waitspanDist);
        //line-specifics
        p->length = partRealDist(pat.length, pat.lengthDist);
        p->deltaLength = partRealDist(pat.deltaLength, pat.deltaLengthDist);
        // CAT: new particle fields copied from the template (partFillGenericParticles 0x004e175c-0x004e17bd)
        p->lengthClamp = pat.lengthClamp;
        p->turbulance = pat.turbulance;
        p->segments = pat.segments;
        if (partTypeof(psys) == PART_LENSFLARES)
        {                                                   //each lens-flare particle owns a lens flare source
            p->lensFlare = memAlloc(sizeof(catlensflaresource), "LensFlareSrc", Pyrophoric);  //exe: 0x40 bytes, flags 4 (0x004e1789)
            memset(p->lensFlare, 0, sizeof(catlensflaresource));   //HWI-FIX: the exe leaves the unset fields as allocated
            p->lensFlare->enabled = 1;
            p->lensFlare->brightness = 1.0f;
            p->lensFlare->state = 1;
            p->lensFlare->visible = 1;
            cat_rndLensFlareSourceAdd(p->lensFlare);
        }
        else
        {
            p->lensFlare = NULL;
        }

        p->bias[0] = partRealDist(pat.rbias, pat.rbiasDist);
        p->bias[1] = partRealDist(pat.gbias, pat.gbiasDist);
        p->bias[2] = partRealDist(pat.bbias, pat.bbiasDist);

        //mesh specifics
        p->mesh = pat.mesh;
        p->tumble[0] = partRealDist(pat.tumble[0], pat.tumbleDist[0]);
        p->tumble[1] = partRealDist(pat.tumble[1], pat.tumbleDist[1]);
        p->tumble[2] = partRealDist(pat.tumble[2], pat.tumbleDist[2]);
        p->deltaTumble[0] = partRealDist(pat.deltaTumble[0], pat.deltaTumbleDist[0]);
        p->deltaTumble[1] = partRealDist(pat.deltaTumble[1], pat.deltaTumbleDist[1]);
        p->deltaTumble[2] = partRealDist(pat.deltaTumble[2], pat.deltaTumbleDist[2]);
        p->loopCount = (sbyte)pat.loopCount;
#if PART_TEST_MORPH
        if (partTypeof(psys) == PART_MESH)
        {
            p->mstruct = partSetupTestMeshMorphBlock();
        }
        else
#endif
        {
            p->mstruct = pat.mstruct;
        }

        p->meshFrame = pat.startMeshFrame;

        if (bitTest(pat.flags, PART_XYZSCALE))
        {
            bitSet(p->flags, PART_XYZSCALE);
            p->tumble[0] = pat.xScale;
            p->tumble[1] = pat.yScale;
            p->tumble[2] = pat.zScale;
        }
        else
        {
            bitClear(p->flags, PART_XYZSCALE);
        }

        if (isWorldspace)
        {
            vector newVector, worldPosition;
            vector velVec, accelVec;
            vector velLOF, velR;
            vector accelLOF, accelR;
            matrix coordSys;
            //initial setup
            memcpy(&coordSys, pat.mat, 9*sizeof(real32));
            memcpy(&worldPosition, pat.position, sizeof(vector));
            //setup velocity
            velLOF.x = velLOF.y = 0.0f;
            velLOF.z = p->velLOF;
            velR = p->rvec;
            vecMultiplyByScalar(velR, p->velR);
            //setup acceleration
            accelLOF.x = accelLOF.y = 0.0f;
            accelLOF.z = p->deltaVelLOF;
            accelR = p->rvec;
            vecMultiplyByScalar(accelR, p->deltaVelR);
            //get velocity
            vecAdd(velVec, velLOF, velR);
            //get acceleration
            vecAdd(accelVec, accelLOF, accelR);
            //transform position
            matMultiplyMatByVec(&newVector, &coordSys, &p->position);
            vecAddTo(newVector, worldPosition);
            p->position = newVector;
            //transform velocity
            matMultiplyMatByVec(&newVector, &coordSys, &velVec);
            vecAddTo(newVector, pat.worldVel);
            p->wVel = newVector;
            p->effectVel = pat.effectVelocity;              // CAT: 0x004e1fd1, the effect's velocity at creation
            //transform acceleration
            matMultiplyMatByVec(&newVector, &coordSys, &accelVec);
            p->wAccel = newVector;
            //transform rvec
            matMultiplyMatByVec(&newVector, &coordSys, &p->rvec);

            vecNormalize(&p->rvec);
        }
        else
        {
            // HWI-FIX: the exe writes effectVel only for worldspace systems, but its non-worldspace pseudo-billboard
            //          renderer (calcPseudoBillboardMatrixNotWorld 0x004dd890) reads it; start it at zero instead of
            //          leaving the allocation's previous contents there
            vecZeroVector(p->effectVel);
        }
    }
    partCreateCallback = NULL;
}

psysPtr partCreateSystemWithDelta(particleType t, udword n, udword delta)
{
    real32 d = (real32)delta;
    real32 rn = (real32)n;
    rn = partRealDist(rn, d);
    dbgAssertOrIgnore (rn > 0);	// otherwise casting to (udword) will wreak havoc...
    return partCreateSystem(t, (udword)rn);
}

psysPtr partCreateSphericalSystemWithDelta(particleType t, udword n, udword delta)
{
    real32 d = (real32)delta;
    real32 rn = (real32)n;
    rn = partRealDist(rn, d);
    dbgAssertOrIgnore (rn > 0);	// otherwise casting to (udword) will wreak havoc...
    return partCreateSphericalSystem(t, (udword)rn);
}

psysPtr partCreationHelper(particleType t, udword n, udword dist)
{
    psysPtr p;
    pointSystem *point;
    cubeSystem  *cube;
    lineSystem  *line;
    meshSystem  *mesh;
    billSystem  *bill;
    udword len;
    bool32 isWorldspace;

    isWorldspace = (bool32)bitTest(pat.flags, PART_WORLDSPACE);

    g_Billboarded = FALSE;
    switch (t)
    {
    case PART_BILLBOARD:
        len = sizeof(billSystem) + n*sizeof(particle);
        p = memAlloc(len, "ps(partsys)", Pyrophoric);
        bill = (billSystem*)p;
        bill->t = t;
        bill->n = (uword)n;
        bill->lastUpdated = universe.totaltimeelapsed;
        bill->drag = pat.drag;
        bill->isAlive = TRUE;
        bill->flags = pat.flags;
        bill->ang = pat.ang;
        bill->angDelta = pat.angDelta;
        bill->tex = pat.tex;
        bill->slices = (uword)pat.slices;
        bill->partMat = pat.mat;
        memcpy(bill->uv, pat.uv, 4*sizeof(real32));
        memcpy(&bill->position, pat.position, sizeof(vector));
        g_Billboarded = TRUE;
        partFillGenericParticles(n, p, sizeof(billSystem), dist, isWorldspace);
        break;
    case PART_MESH:
        len = sizeof(meshSystem) + n*sizeof(particle);
        p = memAlloc(len, "ps(partsys)", Pyrophoric);
        mesh = (meshSystem*)p;
        mesh->t = t;
        mesh->n = (uword)n;
        mesh->lastUpdated = universe.totaltimeelapsed;
        mesh->drag = pat.drag;
        mesh->isAlive = TRUE;
        mesh->flags = pat.flags;
        mesh->ang = pat.ang;
        mesh->angDelta = pat.angDelta;
        mesh->tex = pat.tex;
        mesh->partMat = pat.mat;
        memcpy(&mesh->partPos, pat.position, sizeof(vector));
        partFillGenericParticles(n, p, sizeof(meshSystem), dist, isWorldspace);
        break;
    case PART_LINES:
        len = sizeof(lineSystem) + n*sizeof(particle);
        p = memAlloc(len, "ps(partsys)", Pyrophoric);
        line = (lineSystem*)p;
        line->t = t;
        line->n = (uword)n;
        line->lastUpdated = universe.totaltimeelapsed;
        line->drag = pat.drag;
        line->isAlive = TRUE;
        line->flags = pat.flags;
        line->ang = pat.ang;
        line->angDelta = pat.angDelta;
        partFillGenericParticles(n, p, sizeof(lineSystem), dist, isWorldspace);
        break;
    case PART_CUBES:
        len = sizeof(cubeSystem) + n*sizeof(particle);
        p = memAlloc(len, "ps(partsys)", Pyrophoric);
        cube = (cubeSystem*)p;
        cube->t = t;
        cube->n = (uword)n;
        cube->lastUpdated = universe.totaltimeelapsed;
        cube->drag = pat.drag;
        cube->isAlive = TRUE;
        cube->flags = pat.flags;
        cube->ang = pat.ang;
        cube->angDelta = pat.angDelta;
        partFillGenericParticles(n, p, sizeof(cubeSystem), dist, isWorldspace);
        break;
    case PART_POINTS:
        len = sizeof(pointSystem) + n*sizeof(particle);
        p = memAlloc(len, "ps(partsys)", Pyrophoric);
        point = (pointSystem*)p;
        point->t = t;
        point->n = (uword)n;
        point->lastUpdated = universe.totaltimeelapsed;
        point->drag = pat.drag;
        point->isAlive = TRUE;
        point->flags = pat.flags;
        point->ang = pat.ang;
        point->angDelta = pat.angDelta;
        partFillGenericParticles(n, p, sizeof(pointSystem), dist, isWorldspace);
        break;
    case PART_EBOLTS:
        // CAT: electric bolts (partCreationHelper 0x004e2150 case 5): the point-system header plus the template's
        //      turbulance and segments
        {
            eboltSystem *ebolt;
            len = sizeof(eboltSystem) + n*sizeof(particle);
            p = memAlloc(len, "ps(partsys)", Pyrophoric);
            ebolt = (eboltSystem*)p;
            ebolt->t = t;
            ebolt->n = (uword)n;
            ebolt->lastUpdated = universe.totaltimeelapsed;
            ebolt->drag = pat.drag;
            ebolt->isAlive = TRUE;
            ebolt->flags = pat.flags;
            ebolt->ang = pat.ang;
            ebolt->angDelta = pat.angDelta;
            ebolt->turbulance = pat.turbulance;
            ebolt->segments = pat.segments;
            partFillGenericParticles(n, p, sizeof(eboltSystem), dist, isWorldspace);
        }
        break;
    case PART_LENSFLARES:
        // CAT: lens flares (case 6): a point-system header; partFillGenericParticles gives every particle a source
        len = sizeof(pointSystem) + n*sizeof(particle);
        p = memAlloc(len, "ps(partsys)", Pyrophoric);
        point = (pointSystem*)p;
        point->t = t;
        point->n = (uword)n;
        point->lastUpdated = universe.totaltimeelapsed;
        point->drag = pat.drag;
        point->isAlive = TRUE;
        point->flags = pat.flags;
        point->ang = pat.ang;
        point->angDelta = pat.angDelta;
        partFillGenericParticles(n, p, sizeof(pointSystem), dist, isWorldspace);
        break;
    default:
#if PART_Verbose > 0
        dbgMessagef("unknown particle system type %d", (udword)t);
#endif
        p = NULL;
    }

#if PART_Verbose > 1
    dbgMessagef("system required %dK", len >> 10);
#endif

    return p;
}

/*-----------------------------------------------------------------------------
    Name        : partCreateSphericalSystem
    Description : creates a particle system with the current state,
                  but in the shape of a sphere
    Inputs      : t - type of particles in the system
                  n - number of particles
    Outputs     :
    Return      : returns the psysPtr, or NULL on error
----------------------------------------------------------------------------*/
psysPtr partCreateSphericalSystem(particleType t, udword n)
{
    return partCreationHelper(t, n, PART_SPHERE);
}

/*-----------------------------------------------------------------------------
    Name        : partCreateSystem
    Description : creates a particle system with the current state
    Inputs      : t - type of particles in the system
                  n - number of particles
    Outputs     :
    Return      : returns the psysPtr or NULL on error
----------------------------------------------------------------------------*/
psysPtr partCreateSystem(particleType t, udword n)
{
    return partCreationHelper(t, n, PART_CYLINDER);
}

/*-----------------------------------------------------------------------------
    Name        : partHeaderSize
    Description : returns the size of the header of a system of particles
    Inputs      : psys - the particle system
    Outputs     :
    Return      : returns the header size or 0 if of unrecognized type
----------------------------------------------------------------------------*/
udword partHeaderSize(psysPtr psys)
{
    pointSystem *p = (pointSystem*)psys;
    switch (p->t)
    {
    case PART_POINTS:
        return sizeof(pointSystem);
    case PART_CUBES:
        return sizeof(cubeSystem);
    case PART_LINES:
        return sizeof(lineSystem);
    case PART_MESH:
        return sizeof(meshSystem);
    case PART_BILLBOARD:
        return sizeof(billSystem);
    case PART_EBOLTS:                                       // CAT: partHeaderSize 0x004e25a0 case 5 (0x24)
        return sizeof(eboltSystem);
    case PART_LENSFLARES:                                   // CAT: case 6 (0x1c)
        return sizeof(pointSystem);
    default:
        return 0;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_partSystemFreeAttachments
    Description : CAT: new (0x004e2500). Removes the lens flare source of every particle of a PART_LENSFLARES
                  system from the lens flare list and frees it; other systems have no attachments.
    Inputs      : psys - particle system about to be freed
    Outputs     : particle.lensFlare cleared
    Return      :
----------------------------------------------------------------------------*/
void cat_partSystemFreeAttachments(psysPtr psys)
{
    udword i;
    particle *p;

    for (i = 0; i < partNumberof(psys); i++)
    {
        p = (particle*)(psys + partHeaderSize(psys)) + i;
        if (partTypeof(psys) == PART_LENSFLARES)
        {
            if (p->lensFlare != NULL)
            {
                cat_rndLensFlareSourceRemove(p->lensFlare);
                memFree(p->lensFlare);
            }
            p->lensFlare = NULL;
        }
    }
}

/*============================
  modifiers
 ============================*/
/*-----------------------------------------------------------------------------
    CAT: the modify* functions of the ETG function table take one argument array (Particle.h): args[0] is the
    particle system, the rest are the script's arguments. Each loops over the 0x100-byte particles as the exe does
    (partHeaderSize inlined, the HW1 bodies otherwise unless a comment says so).
-----------------------------------------------------------------------------*/
#define CAT_PART_ARGSYS(args)       ((psysPtr)(args)[0])
#define CAT_PART_ARGF(args, i)      MemsizeToReal32((args)[i])
#define CAT_PART_ARGU(args, i)      ((udword)(args)[i])
// CAT: the exe converts colour bytes with a multiplication by the float 1/255 (0x0072a6ac), HW1 divides by 255.0f
#define CAT_PART_UB2R(b)            ((real32)(b) * (1.0f / 255.0f))

void partSetSystemAlphaFlag(psysPtr psys, real32 a)
{
    pointSystem* sys = (pointSystem*)psys;

    if (a > 0.996f)
    {
        bitClear(sys->flags, PART_ALPHA);
    }
    else
    {
        bitSet(sys->flags, PART_ALPHA);
    }
}

// CAT: partModifyTumble 0x004e25f0: args psys, x, y, z; sets the tumble of every particle of any system type (HW1
//      took a vector pointer and refused anything but PART_MESH with a debug message; the exe has no such test).
memsize partModifyTumble(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 x = CAT_PART_ARGF(args, 1), y = CAT_PART_ARGF(args, 2), z = CAT_PART_ARGF(args, 3);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));

    for (i = 0; i < partNumberof(psys); i++, p++)
    {
        p->tumble[0] = x;
        p->tumble[1] = y;
        p->tumble[2] = z;
    }
    return 0;
}

void partModifyDeltaTumble(psysPtr psys, vector* dt)
{
    if (partTypeof(psys) == PART_MESH)
    {
        udword i;
        particle *p = (particle*)(psys + partHeaderSize(psys));

        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            memcpy(p->deltaTumble, dt, sizeof(vector));
        }
    }
    else
    {
        dbgMessage("particle system is not of type PART_MESH");
    }
}

memsize partModifyExponent(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 exponent = CAT_PART_ARGF(args, 1);
    udword i;
    udword fSpec = (exponent == 0.0f) ? 0 : PART_SPECULAR;

    particle* p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
    {
        p->exponent = exponent;
        p->flags = ((p->flags & (~PART_SPECULAR)) | fSpec);
    }
    return 0;
}

void partModifyDrag(psysPtr psys, real32 d)
{
    pointSystem *p = (pointSystem*)psys;
    if (fabs((real64)d) < 0.0001)
        d = 0.0f;
    if (d < 0.0f || d > 1.0f)
        dbgMessagef("drag coefficient out of range (%f)", d);
    else
        p->drag = d;
}

memsize partModifyMesh(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    meshdata *mesh = (meshdata *)args[1];
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));

    for (i = 0; i < partNumberof(psys); i++, p++)
    {
        p->mesh = mesh;
    }
    return 0;
}

memsize partModifyLighting(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    bool8 lit = (bool8)args[1];                             //the exe stores the low byte (0x004e2875)
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
        p->lit = lit;
    return 0;
}

memsize partModifyIllum(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 illum = CAT_PART_ARGF(args, 1);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
        p->illum = illum;
    return 0;
}

memsize partModifyDeltaIllum(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 deltaIllum = CAT_PART_ARGF(args, 1);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
        p->deltaIllum = deltaIllum;
    return 0;
}

// CAT: partModifyTexture 0x004e29b0: args psys, tex, u0, v0, u1, v1 (setTexture/modifyTexture callable and
//      partUpdateSystem's texture animation)
memsize partModifyTexture(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    trhandle tex = (trhandle)args[1];
    udword u0 = CAT_PART_ARGU(args, 2), v0 = CAT_PART_ARGU(args, 3), u1 = CAT_PART_ARGU(args, 4), v1 = CAT_PART_ARGU(args, 5);
    billSystem *m = (billSystem*)psys;
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    if (m->t == PART_BILLBOARD)
    {
        m->tex = tex;
        m->uv[0][0] = u0;
        m->uv[0][1] = v0;
        m->uv[1][0] = u1;
        m->uv[1][1] = v1;
        for (i = 0; i < partNumberof(psys); i++, p++)
            p->tex = tex;
    }
    else if (m->t == PART_MESH)
    {
        ((meshSystem *)psys)->tex = tex;
        for (i = 0; i < partNumberof(psys); i++, p++)
            p->tex = tex;
    }
    else
    {
        dbgMessage("particle system is not of type PART_BILLBOARD");
    }
    return 0;
}

// CAT: partModifyScale 0x004e2ac0 sets both the X scale (+0x50) and the new Y scale (+0x54)
memsize partModifyScale(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 s = CAT_PART_ARGF(args, 1);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
    {
        p->scale = s;
        p->scaleY = s;
    }
    return 0;
}

// CAT: new, cat_partModifyScaleX 0x004e2b40 (modifyScaleX): the X scale only
memsize cat_partModifyScaleX(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 s = CAT_PART_ARGF(args, 1);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
        p->scale = s;
    return 0;
}

// CAT: new, cat_partModifyScaleY 0x004e2bc0 (modifyScaleY): the Y scale only
memsize cat_partModifyScaleY(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 s = CAT_PART_ARGF(args, 1);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
        p->scaleY = s;
    return 0;
}

memsize partModifyDeltaScale(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 d = CAT_PART_ARGF(args, 1);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
        p->deltaScale = d;
    return 0;
}

// CAT: partModifyDeltaLength 0x004e2cc0 takes a 3rd argument (table types int, float, int) that it stores in the new
//      particle.lengthClamp (+0x70); partUpdateSystem stops the length at 0 when it is set
memsize partModifyDeltaLength(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 d = CAT_PART_ARGF(args, 1);
    bool32 clamp = (bool32)CAT_PART_ARGU(args, 2);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
    {
        p->deltaLength = d;
        p->lengthClamp = clamp;
    }
    return 0;
}

memsize partModifyLifespan(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 l = CAT_PART_ARGF(args, 1);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
        p->lifespan = l;
    return 0;
}

memsize partModifyLength(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 l = CAT_PART_ARGF(args, 1);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
        p->length = l;
    return 0;
}

memsize partModifyVelLOF(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 v = CAT_PART_ARGF(args, 1);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
        p->velLOF = v;
    return 0;
}

memsize partModifyDeltaVelLOF(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 d = CAT_PART_ARGF(args, 1);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
        p->deltaVelLOF = d;
    return 0;
}

memsize partModifyVelR(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 v = CAT_PART_ARGF(args, 1);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
        p->velR = v;
    return 0;
}

// CAT: new, cat_partModifyRotAngle 0x004e2fd0 (modifyRotAngle): the sprite rotation (+0x84) of every particle
memsize cat_partModifyRotAngle(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 rot = CAT_PART_ARGF(args, 1);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
        p->rot = rot;
    return 0;
}

memsize partModifyDeltaVelR(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 d = CAT_PART_ARGF(args, 1);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
        p->deltaVelR = d;
    return 0;
}

// CAT: new, cat_partModifyAlphaValue 0x004e30d0 (modifyAlphaValue): the alpha (+0x3c) of every particle; the system's
//      PART_ALPHA flag follows the alpha as in partSetSystemAlphaFlag (0x004e310e: alpha <= 0.996 (0x0072afc0) sets
//      it, else it is cleared) [audit: the clear jumps past the "or 2" (0x004e3121), so the flag is not always set]
memsize cat_partModifyAlphaValue(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 a = CAT_PART_ARGF(args, 1);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));

    partSetSystemAlphaFlag(psys, a);
    for (i = 0; i < partNumberof(psys); i++, p++)
        p->icolor[3] = a;
    return 0;
}

void partModifyColor(psysPtr psys, real32 r, real32 g, real32 b, real32 a)
{
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));

    partSetSystemAlphaFlag(psys, a);

    for (i = 0; i < partNumberof(psys); i++, p++)
    {
        p->icolor[0] = r;
        p->icolor[1] = g;
        p->icolor[2] = b;
        p->icolor[3] = a;
    }
}

// CAT: partModifyColorC 0x004e3170 (modifyColor, modifyColorA): args psys, color; PART_ALPHA follows the alpha as in
//      HW1 (0x004e3212-0x004e3227) [audit: not always set; the clear jumps past the "or 2" (0x004e3225)]
memsize partModifyColorC(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    color c = (color)args[1];
    udword i;
    real32 r, g, b, a;

#if FIX_ENDIAN
    c = FIX_ENDIAN_INT_32(c);
#endif

    particle *p = (particle*)(psys + partHeaderSize(psys));

    r = CAT_PART_UB2R(colRed(c));
    g = CAT_PART_UB2R(colGreen(c));
    b = CAT_PART_UB2R(colBlue(c));
    a = CAT_PART_UB2R(colAlpha(c));

    partSetSystemAlphaFlag(psys, a);

    for (i = 0; i < partNumberof(psys); i++, p++)
    {
        p->icolor[0] = r;
        p->icolor[1] = g;
        p->icolor[2] = b;
        p->icolor[3] = a;
    }
    return 0;
}

void partModifyDeltaColorC(psysPtr psys, color c)
{
    udword i;
    real32 r, g, b, a;

#if FIX_ENDIAN
    c = FIX_ENDIAN_INT_32(c);
#endif

    particle *p = (particle*)(psys + partHeaderSize(psys));

    r = colUbyteToReal(colRed(c));
    g = colUbyteToReal(colGreen(c));
    b = colUbyteToReal(colBlue(c));
    a = colUbyteToReal(colAlpha(c));

    for (i = 0; i < partNumberof(psys); i++, p++)
    {
        p->deltaColor[0] = r;
        p->deltaColor[1] = g;
        p->deltaColor[2] = b;
        p->deltaColor[3] = a;
    }
}

memsize partModifyDeltaColor(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 dr = CAT_PART_ARGF(args, 1), dg = CAT_PART_ARGF(args, 2), db = CAT_PART_ARGF(args, 3), da = CAT_PART_ARGF(args, 4);
    udword i;
    particle *p = (particle*)(psys + partHeaderSize(psys));

    for (i = 0; i < partNumberof(psys); i++, p++)
    {
        p->deltaColor[0] = dr;
        p->deltaColor[1] = dg;
        p->deltaColor[2] = db;
        p->deltaColor[3] = da;
    }
    return 0;
}

memsize partModifyAnimation(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    void *tstruct = (void *)args[1];

    if (partTypeof(psys) == PART_BILLBOARD ||
        partTypeof(psys) == PART_MESH)
    {
        udword i;
        particle* p = (particle*)(psys + partHeaderSize(psys));
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            p->tstruct = tstruct;
        }
    }
    else
    {
        dbgMessage("partModifyAnimation needs a PART_BILLBOARD or PART_MESH");
    }
    return 0;
}

memsize partModifyMorph(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    void *mstruct = (void *)args[1];

    if (partTypeof(psys) == PART_MESH)
    {
        udword i;
        particle* p = (particle*)(psys + partHeaderSize(psys));
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            p->mstruct = mstruct;
        }
    }
    else
    {
        dbgMessage("partModifyMorph needs a PART_MESH");
    }
    return 0;
}

memsize partModifyFramerate(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 rate = CAT_PART_ARGF(args, 1);

    if (partTypeof(psys) == PART_BILLBOARD)
    {
        udword i;
        particle* p = (particle*)(psys + partHeaderSize(psys));
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            p->rate = rate;
        }
    }
    else
    {
        dbgMessage("partModifyFramerate needs a PART_BILLBOARD");
    }
    return 0;
}

memsize partModifyMorphFramerate(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 meshRate = CAT_PART_ARGF(args, 1);

    if (partTypeof(psys) == PART_MESH)
    {
        udword i;
        particle* p = (particle*)(psys + partHeaderSize(psys));
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            p->meshRate = meshRate;
        }
    }
    else
    {
        dbgMessage("partModifyMorphFramerate needs a PART_MESH");
    }
    return 0;
}

void partModifyBillPosition(psysPtr psys, vector* pos)
{
    if (partTypeof(psys) == PART_BILLBOARD)
    {
        billSystem* bpart = (billSystem*)psys;
        bpart->position = *pos;
    }
    else
    {
        dbgMessage("partModifyBillPosition needs a PART_BILLBOARD");
    }
}

memsize partModifyColorBias(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    real32 rb = CAT_PART_ARGF(args, 1), gb = CAT_PART_ARGF(args, 2), bb = CAT_PART_ARGF(args, 3);
    udword i;
    particle* p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
    {
        p->bias[0] = rb;
        p->bias[1] = gb;
        p->bias[2] = bb;
    }
    return 0;
}

memsize partModifyAddColor(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    color c = (color)args[1];
    udword i;

#if FIX_ENDIAN
    c = FIX_ENDIAN_INT_32(c);
#endif

    real32 r = CAT_PART_UB2R(colRed(c));
    real32 g = CAT_PART_UB2R(colGreen(c));
    real32 b = CAT_PART_UB2R(colBlue(c));
    particle* p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
    {
        p->bias[0] = r;
        p->bias[1] = g;
        p->bias[2] = b;
    }
    return 0;
}

void partModifyLoopFlag(psysPtr psys, bool8 willLoop)
{
    if (partTypeof(psys) == PART_BILLBOARD)
    {
        udword i;
        particle* p = (particle*)(psys + partHeaderSize(psys));
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            p->loop = willLoop;
        }
    }
    else
    {
        dbgMessage("partModifyLoopFlag needs a PART_BILLBOARD");
    }
}

void partModifySpecular(psysPtr psys, bool32 spec)
{
    udword i;
    particle* p;

    p = (particle*)(psys + partHeaderSize(psys));

    if (spec)
    {
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            bitSet(p->flags, PART_SPECULAR);
        }
    }
    else
    {
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            bitClear(p->flags, PART_SPECULAR);
        }
    }
}

void partModifyStipple(psysPtr psys, bool32 stip)
{
    udword i;
    particle* p;

    p = (particle*)(psys + partHeaderSize(psys));

    if (stip)
    {
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            bitSet(p->flags, PART_STIPPLE);
        }
    }
    else
    {
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            bitClear(p->flags, PART_STIPPLE);
        }
    }
}

void partModifyNoDepthWrite(psysPtr psys, bool32 noWrite)
{
    udword i;
    particle* p;

    p = (particle*)(psys + partHeaderSize(psys));

    if (noWrite)
    {
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            bitSet(p->flags, PART_NODEPTHWRITE);
        }
    }
    else
    {
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            bitClear(p->flags, PART_NODEPTHWRITE);
        }
    }
}

void partModifyAdditiveBlends(psysPtr psys, bool32 add)
{
    udword i;
    particle* p;

    p = (particle*)(psys + partHeaderSize(psys));

    if (add)
    {
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            bitSet(p->flags, PART_ADDITIVE);
        }
    }
    else
    {
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            bitClear(p->flags, PART_ADDITIVE);
        }
    }
}

memsize partModifyPseudoBillboard(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    bool32 bill = (bool32)CAT_PART_ARGU(args, 1);
    udword i;
    particle* p;

    p = (particle*)(psys + partHeaderSize(psys));

    if (bill)
    {
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            bitSet(p->flags, PART_PSEUDOBILLBOARD);
        }
    }
    else
    {
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            bitClear(p->flags, PART_PSEUDOBILLBOARD);
        }
    }
    return 0;
}

void partModifyTrueBillboard(psysPtr psys, bool32 bill)
{
    udword i;
    particle* p;

    p = (particle*)(psys + partHeaderSize(psys));

    if (bill)
    {
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            bitSet(p->flags, PART_TRUEBILLBOARD);
        }
    }
    else
    {
        for (i = 0; i < partNumberof(psys); i++, p++)
        {
            bitClear(p->flags, PART_TRUEBILLBOARD);
        }
    }
}

memsize partModifyColorScheme(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    sdword colorScheme = (sdword)args[1];
    udword i;
    particle* p;

    p = (particle*)(psys + partHeaderSize(psys));
    for (i = 0; i < partNumberof(psys); i++, p++)
    {
        p->colorScheme = (ubyte)(uword)(udword)colorScheme;
    }
    return 0;
}

//0 - normal, 1 - additive, 2 - stipple
memsize partModifyAlphaMode(memsize *args)
{
    psysPtr psys = CAT_PART_ARGSYS(args);
    udword mode = CAT_PART_ARGU(args, 1);

    switch (mode)
    {
    case 1:
        partModifyAdditiveBlends(psys, TRUE);
        partModifyStipple(psys, FALSE);
        break;
    case 2:
        partModifyAdditiveBlends(psys, FALSE);
        partModifyStipple(psys, TRUE);
        break;
    default:
        partModifyAdditiveBlends(psys, FALSE);
        partModifyStipple(psys, FALSE);
    }
    return 0;
}


/*============================
  callbacks
 ============================*/
/*-----------------------------------------------------------------------------
    CAT: the set* functions of the ETG function table take one argument array (Particle.h); the others keep their
    values. Each stores into the particle template as HW1's did (exe 0x004e39b0-0x004e4362).
-----------------------------------------------------------------------------*/
memsize partSetIsWorldspace(memsize *args)
{
    bool8 isWorldspace = (bool8)args[0];                    //the exe tests the low byte (0x004e39b4)
    if (isWorldspace)
    {
        bitSet(pat.flags, PART_WORLDSPACE);
    }
    else
    {
        bitClear(pat.flags, PART_WORLDSPACE);
    }
    return 0;
}

memsize partSetVelocityInWorldSpace(memsize *args)
{
    bool8 isLocalSpace = (bool8)args[0];
    if (isLocalSpace)
    {
        bitSet(pat.flags, PART_WORLDSPACEVELOCITY);
    }
    else
    {
        bitClear(pat.flags, PART_WORLDSPACEVELOCITY);
    }
    return 0;
}

void partSetExponent(real32 exponent)
{
    pat.exponent = exponent;
}

void patSetExponentDist(real32 exponentDist)
{
    pat.exponentDist = exponentDist;
}

// CAT: partSetWorldVel 0x004e3a00 copies the velocity into worldVel and effectVelocity; HW1 kept a pointer to it in
//      partEffectVelocity for the pseudo-billboard renderers, which now read particle.effectVel instead
void partSetWorldVel(vector* worldVel)
{
    memcpy(&pat.worldVel, worldVel, sizeof(vector));
    memcpy(&pat.effectVelocity, worldVel, sizeof(vector));
}

void partSetAnimation(void* tstruct)
{
    pat.tstruct = tstruct;
}

void partSetMorph(void* mstruct)
{
    pat.mstruct = mstruct;
}

void partSetMorphFramerate(real32 meshRate)
{
    pat.meshRate = meshRate;
}

void partSetMorphLoopCount(sdword loopCount)
{
    pat.loopCount = loopCount;
}

memsize partSetMeshStartFrame(memsize *args)
{
    pat.startMeshFrame = CAT_PART_ARGF(args, 0);
    return 0;
}

void partSetFramerate(real32 rate)
{
    pat.rate = rate;
}

memsize partSetLoopFlag(memsize *args)
{
    pat.loop = (bool8)args[0];
    return 0;
}

memsize partSetStartFrame(memsize *args)
{
    pat.startFrame = (uword)args[0];
    return 0;
}

void partSetCoordSys(matrix* mat)
{
    pat.mat = mat;
}

memsize partSetTumble(memsize *args)
{
    pat.tumble[0] = CAT_PART_ARGF(args, 0);
    pat.tumble[1] = CAT_PART_ARGF(args, 1);
    pat.tumble[2] = CAT_PART_ARGF(args, 2);
    return 0;
}

void partSetTumbleDist(real32 dx, real32 dy, real32 dz)
{
    pat.tumbleDist[0] = dx;
    pat.tumbleDist[1] = dy;
    pat.tumbleDist[2] = dz;
}

memsize partSetDeltaTumble(memsize *args)
{
    pat.deltaTumble[0] = CAT_PART_ARGF(args, 0);
    pat.deltaTumble[1] = CAT_PART_ARGF(args, 1);
    pat.deltaTumble[2] = CAT_PART_ARGF(args, 2);
    return 0;
}

void partSetDeltaTumbleDist(real32 dx, real32 dy, real32 dz)
{
    pat.deltaTumbleDist[0] = dx;
    pat.deltaTumbleDist[1] = dy;
    pat.deltaTumbleDist[2] = dz;
}

memsize partSetSlices(memsize *args)
{
    udword s = CAT_PART_ARGU(args, 0);
    if (s < 2 || s > 64)
    {
        dbgMessage("insane number of slices, using default of 8");
        pat.slices = 8;
    }
    else
        pat.slices = s;
    return 0;
}

void partSetPosition(vector *pos)
{
    memcpy(pat.position, pos, sizeof(vector));
}

// CAT: setMesh's callable (0x004e3b80, etgSetMeshParse 0x0046cdf0): args[0] is the mesh
memsize partSetMeshdata(memsize *args)
{
    pat.mesh = (meshdata *)args[0];
    return 0;
}

// CAT: setTexture's callable (partSetTrHandle 0x004e3b90): args tex, u0, v0, u1, v1
memsize partSetTrHandle(memsize *args)
{
    pat.tex = (trhandle)args[0];
    pat.uv[0][0] = CAT_PART_ARGU(args, 1);
    pat.uv[0][1] = CAT_PART_ARGU(args, 2);
    pat.uv[1][0] = CAT_PART_ARGU(args, 3);
    pat.uv[1][1] = CAT_PART_ARGU(args, 4);
    return 0;
}

void partSetDrag(real32 n)
{
    pat.drag = n;
}

memsize partSetXYZScale(memsize *args)
{
    bitSet(pat.flags, PART_XYZSCALE);
    pat.xScale = CAT_PART_ARGF(args, 0);
    pat.yScale = CAT_PART_ARGF(args, 1);
    pat.zScale = CAT_PART_ARGF(args, 2);
    return 0;
}

memsize partSetScale(memsize *args)
{
    pat.scale = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetScaleDist(memsize *args)
{
    real32 d = CAT_PART_ARGF(args, 0);
    pat.scaleDist = d;
    if (d == 0.0f)
        bitSet(pat.flags, PART_MONOSCALE);
    else
        bitClear(pat.flags, PART_MONOSCALE);
    return 0;
}

memsize partSetDeltaScale(memsize *args)
{
    pat.deltaScale = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetDeltaScaleDist(memsize *args)
{
    pat.deltaScaleDist = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetOffsetLOF(memsize *args)
{
    pat.offsetLOF = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetOffsetR(memsize *args)
{
    pat.offsetR = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetOffsetTheta(memsize *args)
{
    pat.offsetTheta = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetOffsetArray(memsize *args)
{
    pat.offsetArray = (vector *)args[0];
    return 0;
}

// CAT: new, cat_partSetRandomOffsetArray 0x004e3cc0 (setRandomOffsetArray): args x0, y0, z0, x1, y1, z1 bound a box
//      (each pair is ordered first); 100 offsets inside it are drawn from RNG stream 0 (x, y, z per entry:
//      lo + rand * (hi - lo) / 2^32) into a static array at 0x00a505e8, which becomes pat.offsetArray
#define CAT_PART_RANDOM_OFFSETS     100
static vector catPartRandomOffsets[CAT_PART_RANDOM_OFFSETS];
memsize cat_partSetRandomOffsetArray(memsize *args)
{
    real32 lo[3], hi[3], scale[3], t;
    sdword i, j;

    for (j = 0; j < 3; j++)
    {
        lo[j] = CAT_PART_ARGF(args, j);
        hi[j] = CAT_PART_ARGF(args, j + 3);
        if (hi[j] < lo[j])
        {
            t = hi[j]; hi[j] = lo[j]; lo[j] = t;
        }
        scale[j] = (hi[j] - lo[j]) * (1.0f / 4294967296.0f);    //the exe's constant 2^-32 (0x0072a4e0)
    }
    for (i = 0; i < CAT_PART_RANDOM_OFFSETS; i++)
    {
        //x87: fild of the unsigned draw, fmul by the float scale, fadd the float low bound, one rounding to float
        catPartRandomOffsets[i].x = (real32)((long double)ranRandom(RANDOM_STREAM_0) * scale[0] + lo[0]);
        catPartRandomOffsets[i].y = (real32)((long double)ranRandom(RANDOM_STREAM_0) * scale[1] + lo[1]);
        catPartRandomOffsets[i].z = (real32)((long double)ranRandom(RANDOM_STREAM_0) * scale[2] + lo[2]);
    }
    pat.offsetArray = catPartRandomOffsets;
    return 0;
}

memsize partSetDeltaLOF(memsize *args)
{
    pat.deltaLOF = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetDeltaLOFDist(memsize *args)
{
    pat.deltaLOFDist = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetDeltaR(memsize *args)
{
    pat.deltaR[0] = CAT_PART_ARGF(args, 0);
    pat.deltaR[1] = CAT_PART_ARGF(args, 1);
    return 0;
}

memsize partSetDeltaRDist(memsize *args)
{
    real32 t = CAT_PART_ARGF(args, 1);
    pat.deltaRDist[0] = CAT_PART_ARGF(args, 0);
    pat.deltaRDist[1] = (t == 0.0f) ? 6.28f : t;
    return 0;
}

memsize partSetVelLOF(memsize *args)
{
    pat.velLOF = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetVelLOFDist(memsize *args)
{
    pat.velLOFDist = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetVelR(memsize *args)
{
    pat.velR = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetVelRDist(memsize *args)
{
    pat.velRDist = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetDeltaVelLOF(memsize *args)
{
    pat.deltaVelLOF = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetDeltaVelLOFDist(memsize *args)
{
    pat.deltaVelLOFDist = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetDeltaVelR(memsize *args)
{
    pat.deltaVelR = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetDeltaVelRDist(memsize *args)
{
    pat.deltaVelRDist = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetAng(memsize *args)
{
    pat.ang = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetAngDist(memsize *args)
{
    pat.angDist = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetAngDelta(memsize *args)
{
    pat.angDelta = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetAngDeltaDist(memsize *args)
{
    pat.angDeltaDist = CAT_PART_ARGF(args, 0);
    return 0;
}

void partSetColor(real32 r, real32 g, real32 b)
{
    pat.icolor[0] = r;
    pat.icolor[1] = g;
    pat.icolor[2] = b;
    pat.icolor[3] = 1.0f;
    bitClear(pat.flags, PART_ALPHA);
}

memsize partSetColorDist(memsize *args)
{
    pat.colorDist[0] = CAT_PART_ARGF(args, 0);
    pat.colorDist[1] = CAT_PART_ARGF(args, 1);
    pat.colorDist[2] = CAT_PART_ARGF(args, 2);
    pat.colorDist[3] = 0.0f;
    bitClear(pat.flags, PART_ALPHA);
    return 0;
}

void partSetColorA(real32 r, real32 g, real32 b, real32 a)
{
    pat.icolor[0] = r;
    pat.icolor[1] = g;
    pat.icolor[2] = b;
    pat.icolor[3] = a;
    bitSet(pat.flags, PART_ALPHA);
}

memsize partSetColorADist(memsize *args)
{
    pat.colorDist[0] = CAT_PART_ARGF(args, 0);
    pat.colorDist[1] = CAT_PART_ARGF(args, 1);
    pat.colorDist[2] = CAT_PART_ARGF(args, 2);
    pat.colorDist[3] = CAT_PART_ARGF(args, 3);
    bitSet(pat.flags, PART_ALPHA);
    return 0;
}

memsize partSetDeltaColor(memsize *args)
{
    pat.deltaColor[0] = CAT_PART_ARGF(args, 0);
    pat.deltaColor[1] = CAT_PART_ARGF(args, 1);
    pat.deltaColor[2] = CAT_PART_ARGF(args, 2);
    pat.deltaColor[3] = 1.0f;
    bitClear(pat.flags, PART_ALPHA);
    return 0;
}

memsize partSetDeltaColorDist(memsize *args)
{
    pat.deltaColorDist[0] = CAT_PART_ARGF(args, 0);
    pat.deltaColorDist[1] = CAT_PART_ARGF(args, 1);
    pat.deltaColorDist[2] = CAT_PART_ARGF(args, 2);
    pat.deltaColorDist[3] = 0.0f;
    bitClear(pat.flags, PART_ALPHA);
    return 0;
}

memsize partSetDeltaColorA(memsize *args)
{
    pat.deltaColor[0] = CAT_PART_ARGF(args, 0);
    pat.deltaColor[1] = CAT_PART_ARGF(args, 1);
    pat.deltaColor[2] = CAT_PART_ARGF(args, 2);
    pat.deltaColor[3] = CAT_PART_ARGF(args, 3);
    bitSet(pat.flags, PART_ALPHA);
    return 0;
}

memsize partSetDeltaColorADist(memsize *args)
{
    pat.deltaColorDist[0] = CAT_PART_ARGF(args, 0);
    pat.deltaColorDist[1] = CAT_PART_ARGF(args, 1);
    pat.deltaColorDist[2] = CAT_PART_ARGF(args, 2);
    pat.deltaColorDist[3] = CAT_PART_ARGF(args, 3);
    bitSet(pat.flags, PART_ALPHA);
    return 0;
}

memsize partSetLighting(memsize *args)
{
    pat.lit = (bool8)args[0];
    return 0;
}

memsize partSetIllum(memsize *args)
{
    pat.illum = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetIllumDist(memsize *args)
{
    pat.illumDist = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetDeltaIllum(memsize *args)
{
    pat.deltaIllum = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetDeltaIllumDist(memsize *args)
{
    pat.deltaIllumDist = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetLifespan(memsize *args)
{
    pat.lifespan = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetLifespanDist(memsize *args)
{
    pat.lifespanDist = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetWaitspan(memsize *args)
{
    pat.waitspan = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetWaitspanDist(memsize *args)
{
    pat.waitspanDist = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetLength(memsize *args)
{
    pat.length = CAT_PART_ARGF(args, 0);
    return 0;
}

// CAT: new, cat_partSetTurbulance 0x004e4210 (setTurbulance): electric-bolt turbulance
memsize cat_partSetTurbulance(memsize *args)
{
    pat.turbulance = CAT_PART_ARGF(args, 0);
    return 0;
}

// CAT: new, cat_partSetSegments 0x004e4220 (setSegments): electric-bolt segment count (a word)
memsize cat_partSetSegments(memsize *args)
{
    pat.segments = (uword)args[0];
    return 0;
}

memsize partSetLengthDist(memsize *args)
{
    pat.lengthDist = CAT_PART_ARGF(args, 0);
    return 0;
}

// CAT: partSetDeltaLength 0x004e4240 also clears pat.lengthClamp (0x00a504b4)
memsize partSetDeltaLength(memsize *args)
{
    pat.deltaLength = CAT_PART_ARGF(args, 0);
    pat.lengthClamp = FALSE;
    return 0;
}

memsize partSetDeltaLengthDist(memsize *args)
{
    pat.deltaLengthDist = CAT_PART_ARGF(args, 0);
    return 0;
}

memsize partSetColorBias(memsize *args)
{
    pat.rbias = CAT_PART_ARGF(args, 0);
    pat.gbias = CAT_PART_ARGF(args, 1);
    pat.bbias = CAT_PART_ARGF(args, 2);
    return 0;
}

memsize partSetColorBiasDist(memsize *args)
{
    pat.rbiasDist = CAT_PART_ARGF(args, 0);
    pat.gbiasDist = CAT_PART_ARGF(args, 1);
    pat.bbiasDist = CAT_PART_ARGF(args, 2);
    return 0;
}

void partSetSpecular(bool32 spec)
{
    pat.specularOnly = spec;
}

void partSetStipple(bool32 stip)
{
    pat.stippleAlpha = stip;
}

void partSetNoDepthWrite(bool32 noWrite)
{
    pat.noDepthWrite = noWrite;
}

void partSetAdditiveBlends(bool32 add)
{
    pat.additiveBlends = add;
}

memsize partSetPseudoBillboard(memsize *args)
{
    pat.pseudoBillboard = (bool32)CAT_PART_ARGU(args, 0);
    return 0;
}

memsize partSetTrueBillboard(memsize *args)
{
    pat.trueBillboard = (bool32)CAT_PART_ARGU(args, 0);
    return 0;
}

memsize partSetColorScheme(memsize *args)
{
    pat.colorScheme = (ubyte)args[0];
    return 0;
}

//0 - normal, 1 - additive, 2 - stipple
memsize partSetAlphaMode(memsize *args)
{
    switch (CAT_PART_ARGU(args, 0))
    {
    case 1:
        partSetAdditiveBlends(TRUE);
        partSetStipple(FALSE);
        break;
    case 2:
        partSetAdditiveBlends(FALSE);
        partSetStipple(TRUE);
        break;
    default:
        partSetAdditiveBlends(FALSE);
        partSetStipple(FALSE);
    }
    return 0;
}

void partCreateCallbackSet(void (*function)(sdword userValue, ubyte *userData), sdword userValue, ubyte *userData)
{
    partCreateCallback  = function;
    partCreateUserValue = userValue;
    partCreateUserData  = userData;
}

