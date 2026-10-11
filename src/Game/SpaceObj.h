/*=============================================================================
    SPACEOBJ.H: Object-oriented "Space Object" formats

    Created June 1997 by Gary Shaw
=============================================================================*/

#ifndef ___SPACEOBJ_H
#define ___SPACEOBJ_H

#include <stddef.h>

#include "Color.h"
#include "ETG.h"
#include "Globals.h"
#include "LinkedList.h"
#include "LOD.h"
#include "Matrix.h"
#include "MaxMultiplayer.h"
#include "MeshAnim.h"
#include "Nebulae.h"
#include "ObjTypes.h"
#include "SoundStructs.h"
#include "Trails.h"
#include "Vector.h"

/*=============================================================================
    Switches:
=============================================================================*/
#ifdef HW_BUILD_FOR_DEBUGGING

#define SO_ERROR_CHECKING       1               //general error checking
#define SO_VERBOSE_LEVEL        2               //print extra info
#define SO_CLOOGE_SCALE         1               //clooge a largeness factor for the ships

#else

#define SO_ERROR_CHECKING       0               //general error checking
#define SO_VERBOSE_LEVEL        0               //print extra info
#define SO_CLOOGE_SCALE         0               //clooge a largeness factor for the ships

#endif


/*-----------------------------------------------------------------------------
    Gun Object
    Note: the gun object is not an independent object like an asteroid or ship.
          It must be part of a ship object.
-----------------------------------------------------------------------------*/
//flags for the gun object
#define GF_MultiLevelMatrix     0x00000001
// CAT: GunStatic is 0x11C bytes in Cataclysm (HW1 0xFC): four fields after bulletType (+0x0C..+0x18) and a second
//      fire mode after burstWaitTime (+0xF4..+0x100). Offsets from the gun key table 0x008975a0 and from
//      scriptSetGunStatics 0x00526790 (stride 0x11C, slaveDriver at +0x118). gunsoundtype/bulletType stay at +4/+8
//      but no script key sets them any more (SoundType/BulletType are obsolete keys, docs/formats/shipstatic.md).
typedef struct
{
    GunType guntype;
    GunSoundType gunsoundtype;
    BulletType bulletType;      // type of bullet gun fires
    sdword startUpgradeLevel;   // CAT: key StartUpgradeLevel, WEAPON_* (cat_scriptSetWeaponTypeCB 0x00525ff0)
    sdword endUpgradeLevel;     // CAT: key EndUpgradeLevel, WEAPON_* (cat_scriptSetWeaponTypeCB 0x00525ff0)
    udword gunSize;             // CAT: key GunSize, GUNSIZE_* (cat_scriptSetGunSizeCB 0x00526020)
    real32 damageMult;          // CAT: key DamageMult (scriptSetReal32CB)
    udword flags;               //flags for the gun
    udword gunindex;            //index of gun

    vector position;            // position relative to ship

    // for GUN_Gimble
    vector gunnormal;           // normal vector describing gun orientation (rel to ship)
    real32 cosminAngleFromNorm; // cos of min angle required between gunnormal and gunheading
    real32 cosmaxAngleFromNorm; // cos of max angle possible between gunnormal and gunheading

    // for GUN_NewGimble
    matrix defaultgunorientation;
    matrix defaultGunOrientationNonConcat;

    real32 minturnangle,maxturnangle;
    real32 mindeclination,maxdeclination;
    real32 maxanglespeed;
    real32 maxdeclinationspeed;
    real32 angletracking;
    real32 declinationtracking;

    // for GUN_MissileLauncher
    sdword maxMissiles;

    real32 triggerHappy;        // cosine of maximum allowed angle allowed before a fixed gun will shoot
    real32 gunDamageLo[NUM_TACTICS_TYPES];          // low damage potential
    real32 gunDamageHi[NUM_TACTICS_TYPES];          // high damage potential
    real32 baseGunDamageLo;     //based values loaded in from script
    real32 baseGunDamageHi;   //dido, only used before game loads to precalculate different damages
    real32 bulletlength;        // length of bullets fired by gun
    real32 bulletlifetime;      // life time of bullet
    real32 bulletspeed;         // speed of bullet (0 indicates infinite speed)
    real32 bulletrange;         // total distance bullet can travel
    real32 bulletmass;          // mass of bullet gun shoots
    real32 firetime;            // minimum time required before gun can fire again
    real32 burstFireTime;       // time in seconds a gun can burst (if burstFireTime == 0, this gun doesn't fire in bursts)
    real32 burstWaitTime;       // time in seconds gun must cool down after a burst

    // CAT: second fire mode, keys BulletLifeTime2/BulletSpeed2/BulletRange2/FireTime2 (gun table 0x008975a0)
    real32 bulletlifetime2;
    real32 bulletspeed2;
    real32 bulletrange2;
    real32 firetime2;

    real32 barrelLength;        // length of gun barrel for offsetting muzzle flashes
    real32 recoilLength;        // amount of recoil on the gun when it fires

    //additional stuff for use only in the R1 Heavy Cruiser main ion guns
    vector offset;              //offset to rotate about gun's pivot point
    sdword slaveDriver;         //for slaving guns together

} GunStatic;

typedef struct
{
    NAVLightType lightType;      // type of nav light
    vector position;             // position relative to ship
    rgbquad color;               // color of nav light
    real32 flashrateon;          // delay in seconds each flash is on.
    real32 flashrateoff;         // delay in seconds each flash is off.
    real32 startdelay;             // time to wait before starting.
    real32 size;                 // size of light in meters.
    udword minLOD;               // min LOD to draw light at.
    char name[20];               // name of nav light
    memsize texturehandle;      // handle to a texture (if any).		// actually trhandle, but not defined here
    // CAT: per LOD level, the index of the mesh polygon object the light moves with, 0xFF = none (written by
    //      mexGetNAVLightStaticInfo 0x004b7680 at +0x40 + level; the exe's NAVLightStatic is 0x48 bytes, HW1 0x40)
    ubyte navlightObject[8];
} NAVLightStatic;

typedef struct
{
    vector position;            // position relative to ship
    vector normal;              // normal vector (rel to ship)
} DamageLightStatic;

typedef struct
{
    vector position;            // position relative to ship
    vector nozzlenormal;        // normal vector describing nozzle orientation (rel to ship)
} ResNozzleStatic;

typedef struct
{
    vector position;            // position relative to ship
} RepairNozzleStatic;

typedef struct
{
    vector position;            // position relative to ship
    vector nozzlenormal;        // normal vector describing nozzle orientation (rel to ship)
} TractorBeamStatic;

// types for mine formations

#define PULSE_START 0
#define PULSE_SPAWN_EFFECT 1
#define PULSE_MONITOR_EFFECT 2
#define PULSE_REDIRECT 3

typedef struct
{
    Node FormLink;
    LinkedList MineList;
    vector pulse_dir;               //stores pulses current location
    vector effect_going_to;
    real32 waittime;
    struct Player *playerowner;
    bool32 FULL;
    sdword wallstate;
    struct Effect *effect;
    udword numat;
} MineFormationInfo;

typedef struct
{
    // for GUN_Gimble
    vector gunheading;          // gun heading (relative to ship)

    // for GUN_NewGimble
    matrix curgunorientation;
    matrix curGunOrientationNonConcat;
    real32 angle;
    real32 declination;
    real32 anglespeed;
    real32 declinationspeed;

    // for GUN_MissileLauncher
    sdword numMissiles;

    sdword gimblehandle;

    real32 lasttimefired;       // last time gun was fired
    real32 burstStartTime;      // last time gun started burst
    real32 useBurstFireTime;    // actual burst fire/wait times to use after modified (optimization)
    real32 useBurstWaitTime;
    udword burstState;          // gun burst state variable
    GunStatic *gunstatic;       // pointer to static information
} Gun;

typedef struct
{
    udword lightstate;
    real32 lastTimeFlashed;
    NAVLightStatic *navlightstatic;
} NAVLight;

typedef struct
{
    sdword numGuns;
    Gun guns[1];
} GunInfo;

typedef struct
{
    sdword numLights;
    NAVLight navLights[1];
} NAVLightInfo;

typedef struct
{
    sdword numGuns;
    GunStatic gunstatics[1];
} GunStaticInfo;

typedef struct
{
    sdword numNAVLights;
    NAVLightStatic navlightstatics[1];
} NAVLightStaticInfo;

/*  -------------------------------------------------------
    Static Salvaging Info
    - used to define latch on points for salvage
      capturing vessels
    ------------------------------------------------------- */

//maximum number of ships that can be captured and waiting inside
//a mothership or carrier at any one time to pop out after being
//retrofitted
#define SALVAGE_MAX_CAPTURABLE  50

typedef struct
{
    udword number;
    SalvagePointType type;
    vector position;
    vector conenormal;
    real32 coneangle;
    char name[20];
}SalvageStaticPoint;

typedef struct
{
    sdword numSalvagePoints;
    sdword numNeededForSalvage;
    sdword needBigR1;
    sdword needBigR2;
    sdword willFitCarrier;
    SalvageStaticPoint headingPoint;
    SalvageStaticPoint salvageStaticPoints[1];   //ragged array
} SalvageStaticInfo;

//Non static salvage Information
typedef struct
{
    sdword busy;
    SalvageStaticPoint *salvageStaticPoint;
}SalvagePoint;

typedef struct
{
    sdword numSalvagePoints;
    sdword numSalvagePointsFree;
    sdword numNeededForSalvage;
    SalvagePoint salvagePoints[1];   //ragged array
} SalvageInfo;

#define sizeofSalvageStaticInfo(x)  sizeof(SalvageStaticPoint)*(x-1)+sizeof(SalvageStaticInfo)
#define sizeofSalvageInfo(x)  sizeof(SalvagePoint)*(x) +sizeof(SalvageInfo)

/*-----------------------------------------------------------------------------
    Static Docking Information
    - not an independent object, but part of ShipStaticInfo
-----------------------------------------------------------------------------*/
#define LIGHTNAME_LENGTH    20
typedef struct
{
  ShipType shiptype;
  ShipRace shiprace;
  sword useNewOrientation;
  uword heading;
  uword up;
  sword useNewOffset;
  vector offset;    //offset modification
  bool32 lightNameUsed;
  char lightName[LIGHTNAME_LENGTH];
} DockStaticOveride;

typedef struct
{
  sdword numDockOverides;
  DockStaticOveride dockOverides[1];
} DockOverideInfo;


typedef struct
{
    DockPointType type;
    sdword dockindex;
    vector position;
    vector conenormal;
    real32 coneangle;
    real32 flyawaydist;
    real32 mindist;
    real32 maxdist;
    udword headingdirection;
    udword updirection;
    char name[LIGHTNAME_LENGTH];
} DockStaticPoint;

// CAT: Cataclysm's DockStaticInfo has a 0x50-byte header before the points (HW1: numDockPoints only).
//      scriptSetDockStatics 0x005270c0 allocates 0x50 + n * 0x4C bytes, stores n at +0x48 and then indexes the
//      points by type: firstOfType[t] = index of the first point of type t (0xFF if none), numOfType[t] = how many.
//      InitStatShipInfo 0x0053bab0 stores the point named "center" at +0x4C (cat_dockFindDockStaticPoint 0x00463d00).
typedef struct
{
    ubyte firstOfType[NUM_DOCKPOINT_TYPES];     /*+0x00 CAT: 0xFF = no point of this type */
    ubyte numOfType[NUM_DOCKPOINT_TYPES];       /*+0x23 CAT */
    sdword numDockPoints;                       /*+0x48 */
    DockStaticPoint *centerDockPoint;           /*+0x4C CAT: the "center" point, or NULL */
    DockStaticPoint dockstaticpoints[1];        /*+0x50 */
} DockStaticInfo;

/*-----------------------------------------------------------------------------
    Docking Information
    - not an independent object, but part of Ship
-----------------------------------------------------------------------------*/

typedef struct
{
    DockStaticPoint *dockstaticpoint;
    udword thisDockBusy;
} DockPoint;

typedef struct
{
    sdword busyness;
    sdword numDockPoints;
    DockPoint dockpoints[1];
} DockInfo;

typedef struct
{
    Node node;
    struct Ship *ship;
} InsideShip;

typedef struct
{
    LinkedList insideList;
    sword      FightersInsideme;
    sword      CorvettesInsideme;
//    LinkedList launchList;
} ShipsInsideMe;

//CLAMPING INFO

// CAT: ClampInfo is 0x38 bytes (HW1 0x34): clampObjToObj 0x004346e0 allocates memAlloc(0x38), reuses an existing
//      clampInfo, and keeps on the HOST's clampInfo a pointer at +0x34 to a list of the objects clamped to it (a 0xFA8
//      block: count at +0, objects from +8, i.e. up to 1000; host == NULL for a pure host). The list's code is
//      clampObjToObj's (M2-R4 combat/docking); only the field and its NULL start are here.
struct CatClampedList;
typedef struct
{
    struct SpaceObjRotImpTarg *host;            // +0x00
    matrix clampCoordsys;                       // +0x04
    vector clampOffset;                         // +0x28
    struct CatClampedList *clampedObjs;         // +0x34 CAT: new (name chosen here)
}ClampInfo;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(ClampInfo) == 0x38, "ClampInfo is 0x38 bytes in cataclysm.exe");
_Static_assert(offsetof(ClampInfo, clampedObjs) == 0x34, "ClampInfo clamped-object list at +0x34 in cataclysm.exe");
#endif

/*=============================================================================
    Object Static Types
=============================================================================*/

/*=============================================================================
    Basic SpaceObjStaticInfo type, from which all StaticInfo objects are derived
=============================================================================*/

typedef struct
{
    real32 approxcollspheresize;    // rough collision sphere size more repesentative of object's actual size, rather
                                    // than worst case size used for trivial rejection
    real32 avoidcollspheresize;     // collision sphere size used for collision avoidance
    real32 avoidcollspherepad;      // collision sphere size used for collision avoidance, with padding

    real32 approxcollmodifier;    // approxcollspheresize = approxcollmodifier * collspheresize
    real32 avoidcollmodifier;     // avoidcollspheresize = avoidcollmodifier * collspheresize

    real32 originalCollSphereSize;  // original collspheresize DO NOT CHANGE THIS IF OBJECT SCALES!
    real32 collspheresize;  // bounding sphere for rough collision detection
    real32 collspheresizeSqr; // square of collspheresize precomputed
    vector collsphereoffset;// bounding sphere offset in object co-ordinates

    vector collrectoffset;  // collision rectangle offset in object co-ordinates (leftdownback corner of cube)
    // CAT: 12 new bytes at StaticCollInfo+0x38 (StaticHeader+0x6C): mexSetCollisionInfo 0x004b7880 stores
    //      (collrectoffset + {up,right,forward}length) * 0.5 here; preciseSelection moves +0x7C -> +0x88
    //      (ShipStaticScriptTable 0x008b2428). Name chosen here, the exe's is unknown.
    vector collrectmid;
    real32 uplength;        // length of rectangle in up direction
    real32 rightlength;     // length of rectangle in right direction
    real32 forwardlength;   // length of rectangle in forward direction
    real32 diagonallength;  // length of rectangle from point 0 to point 6 or rectangle
    udword preciseSelection; //object uses precise selection mechanism if LOD <= this level

    //dimensional scale factors
    real32 collBoxScaleForward;
    real32 collBoxScaleUp;
    real32 collBoxScaleRight;

    real32 collBoxOffsetX;
    real32 collBoxOffsetY;
    real32 collBoxOffsetZ;
} StaticCollInfo;
// CAT: 0x70 bytes in the exe (HW1 0x64: the 12-byte collrectmid is new); no pointers, so on every target
_Static_assert(sizeof(StaticCollInfo) == 0x70, "StaticCollInfo is 0x70 bytes in cataclysm.exe");

#define IF_InfoNeeded           0x01
#define IF_InfoLoaded           0x02
#define IF_InfoWillBeNeeded     0x04
#define IF_NotPostLoaded        0x08

typedef struct
{
    lodinfo *LOD;           // Level-of-detail information
    void *pMexData;         // pointer to MEX data (mesh extensions)
    real32 mass;            // object mass (can be negative for neat effects)
    real32 momentOfInertiaX;// moment of inertia of object about X axis
    real32 momentOfInertiaY;// moment of inertia of object about Y axis
    real32 momentOfInertiaZ;// moment of inertia of object about Z axis
    real32 oneOverMass;            // object mass (can be negative for neat effects)
    real32 oneOverMomentOfInertiaX;// moment of inertia of object about X axis
    real32 oneOverMomentOfInertiaY;// moment of inertia of object about Y axis
    real32 oneOverMomentOfInertiaZ;// moment of inertia of object about Z axis
    real32 maxvelocity;     // maximum velocity of object
    real32 maxrot;          // maximum rotate speed (rad/s) of object
//    bool8  infoNeeded;      //static info is needed for this level and should be loaded
//    bool8  infoLoaded;      //static info has been loaded properly
    ubyte  infoFlags;       //IF_InfoNeeded, IF_InfoLoaded, IF_InfoWillBeNeeded
    bool8  immobile;        // flag indicating object is immobile
    sbyte  rightOfWay;
    StaticCollInfo staticCollInfo;
} StaticInfo;

typedef struct
{
    StaticInfo common;
    real32 maxhealth;
    real32 oneOverMaxHealth;
    real32 minCollDamage;
    real32 maxCollDamage;
} StaticInfoHealth;

#define TRANS_UP                0
#define TRANS_DOWN              1
#define TRANS_RIGHT             2
#define TRANS_LEFT              3
#define TRANS_FORWARD           4
#define TRANS_BACKWARD          5
#define NUM_TRANS_DEGOFFREEDOM  6
#define TRANS_BACK              TRANS_BACKWARD

#define ROT_ABOUTXCCW           0
#define ROT_ABOUTXCW            1
#define ROT_ABOUTYCCW           2
#define ROT_ABOUTYCW            3
#define ROT_ABOUTZCCW           4
#define ROT_ABOUTZCW            5
#define NUM_ROT_DEGOFFREEDOM    6

#define ROT_YAWLEFT             ROT_ABOUTXCCW
#define ROT_YAWRIGHT            ROT_ABOUTXCW
#define ROT_PITCHUP             ROT_ABOUTYCCW
#define ROT_PITCHDOWN           ROT_ABOUTYCW
#define ROT_ROLLRIGHT           ROT_ABOUTZCCW
#define ROT_ROLLLEFT            ROT_ABOUTZCW

#define TURN_ABOUTX             0
#define TURN_ABOUTY             1
#define TURN_ABOUTZ             2
#define NUM_TURN_TYPES          3

#define TURN_YAW                TURN_ABOUTX
#define TURN_PITCH              TURN_ABOUTY
#define TURN_ROLL               TURN_ABOUTZ

#define MAX_NUM_TRAILS          4

typedef struct StaticInfoHealthGuidance
{
    union {
        StaticInfo common;
        StaticInfoHealth _health;
    };
    //these 3 things are static and don't affect a ship after the
    //ship has been built.  They are copied into the NonStats data
    //structure due to a need for them to be nonstatic
    real32 thruststrengthstat[NUM_TRANS_DEGOFFREEDOM];
    real32 rotstrengthstat[NUM_ROT_DEGOFFREEDOM];
    real32 turnspeedstat[NUM_TURN_TYPES];
} StaticInfoHealthGuidance;

typedef struct StaticInfoShipOrDerelict {
    union {
        StaticInfo common;
        StaticInfoHealth health;
    };

    // Duplicates fields of StaticInfoHealthGuidance to keep everything byte-aligned with the previous implementation
    // and maintain savegame compatibility
    struct {
        real32 _thruststrengthstat[NUM_TRANS_DEGOFFREEDOM];
        real32 _rotstrengthstat[NUM_ROT_DEGOFFREEDOM];
        real32 _turnspeedstat[NUM_TURN_TYPES];
    };

    // TODO(type safety, 2026-04-24): Extract into another type (e.g. `Colored`) to mutualize with MissileStaticInfo
    bool8 teamColor[MAX_MULTIPLAYER_PLAYERS]; // what color schemes this can be
    // CAT: no salvageStaticInfo pointer. Cataclysm has no NUMBER_OF_SALVAGE_POINTS key anywhere (strings.csv) and its
    //      ShipStaticInfo has teamColor[8] at +0xF0 (universeDefaultTeamColors 0x0053c940 copies 8 bytes there) and
    //      sinbank at +0xF8 (aishipPrecalcInfo 0x00413bd0), 4 bytes less than HW1. Salvage uses DPT_Salvage dock points.

    // these 3 variables must be next
    real32 sinbank; // variables used by the AIShip routines
    real32 pitchturn;
    real32 pitchdescend;
} StaticInfoShipOrDerelict;

// CAT: HW1 code that read statinfo->salvageStaticInfo reads CAT_SALVAGESTATICINFO(statinfo) instead, which is always
//      NULL: Cataclysm has no HW1-style salvage statics (no NUMBER_OF_SALVAGE_POINTS key, no field). The HW1 salvage
//      paths behind it are therefore dormant until stage 2 ports Cataclysm's salvage system (DPT_Salvage points).
#define CAT_SALVAGESTATICINFO(statinfo) ((void)(statinfo), (SalvageStaticInfo *)NULL)

//info for SpaceObjRotImpTarg objects that was once
//static, but now needs to be non-static
typedef struct
{
    real32 thruststrength[NUM_TRANS_DEGOFFREEDOM];
    real32 rotstrength[NUM_ROT_DEGOFFREEDOM];
    real32 turnspeed[NUM_TURN_TYPES];
} NonStats;

/*=============================================================================
    Ship StaticInfo
=============================================================================*/

#define THISSHIPIS_FIGHTER              0
#define THISSHIPIS_CORVETTE             1
#define THISSHIPIS_RESOURCER            2
#define THISSHIPIS_OTHERNONCAPITALSHIP  3

struct ShipStaticInfo;
struct CommandToDo;

struct SpaceObj;
struct Effect;
struct Bullet;

// CAT: CustShipHeader is 0x8C bytes in Cataclysm (HW1 0x48): no leading shiptype, sizeofShipSpecifics, then 34
//      function slots. InitStatShipInfo 0x0053bab0 copies 0x23 dwords from shipHeaders[type] (0x008b1938) to
//      statinfo+0x108, so slot k is at statinfo+0x108+4k. Slot meanings from the call census and the 51 headers the exe
//      ships (docs/formats/shipstatic.md). CustShipAttack moved from HW1 slot 4 to slot 21. The new slots (9, 12-14,
//      20, 22-34) hold the exe's functions in the 29 reused HW1 headers since M2-R3 ships-a and stay NULL elsewhere;
//      of their callers only slot 13's (ETG.c, M2-R3 etg-particles) is ported, the rest are stage 2. Signatures of the new slots are taken from the exe's Round-5 prototypes of their
//      targets (CarrierOrderChanged, CarrierBulletRemoval, MothershipEffectRemoval, HiveFrigateDockCommand); the
//      others are UNVERIFIED.
typedef struct
{
    udword sizeofShipSpecifics;                                                                    /* +0x00 */
    void (*CustShipStaticInit) (char *directory,char *filename,struct ShipStaticInfo *statinfo);  /* slot 1 */
    void (*CustShipStaticInitPost) (struct ShipStaticInfo *statinfo);                             /* slot 2 */
    void (*CustShipInit) (struct Ship *ship);                                                      /* slot 3 */
    void (*CustShipClose) (struct Ship *ship);                                                     /* slot 4 */
    void (*CustShipFire) (struct Ship *ship, struct SpaceObjRotImpTarg *target);                   /* slot 5 */
    void (*CustShipAttackPassive) (struct Ship *ship,struct Ship *target,bool32 rotate);           /* slot 6 */
    bool32 (*CustShipSpecialActivate) (struct Ship *ship);                                         /* slot 7 */
    bool32 (*CustShipSpecialTarget) (struct Ship *ship,void *custom);                              /* slot 8 */
    void (*CustShipOrderChanged) (struct Ship *ship);                                              /* slot 9, CAT new */
    void (*CustShipHousekeep) (struct Ship *ship);                                                 /* slot 10 */
    void (*CustShipRemoveShipReferences)(struct Ship *ship, struct Ship *shiptoremove);            /* slot 11 */
    void (*CustShipRemoveObjReferences)(struct Ship *ship, struct SpaceObj *objtoremove);          /* slot 12, CAT new */
    void (*CustShipEffectRemoval)(struct Ship *ship, struct Effect *effect);                       /* slot 13, CAT new */
    void (*CustShipBulletRemoval)(struct Ship *ship, struct Bullet *bullet);                       /* slot 14, CAT new */
    void (*CustShipDied)(struct Ship *ship);                                                       /* slot 15 */
    void (*CustShip_PreFix)(struct Ship *ship);                                                    /* slot 16 */
    void (*CustShip_Save)(struct Ship *ship);                                                      /* slot 17 */
    void (*CustShip_Load)(struct Ship *ship);                                                      /* slot 18 */
    void (*CustShip_Fix)(struct Ship *ship);                                                       /* slot 19 */
    bool32 (*CustShipMoveCommand)(struct Ship *ship);                                              /* slot 20, CAT new */
    void (*CustShipAttack) (struct Ship *ship,struct SpaceObjRotImpTarg *target,real32 maxdist);  /* slot 21, CAT moved */
    bool32 (*CustShipDockCommand)(struct Ship *ship, sdword dockType, struct Ship *dockwith);      /* slot 22, CAT new */
    bool32 (*CustShipLaunchCommand)(struct Ship *ship);                                            /* slot 23, CAT new */
    void (*CustShipSlot24)(void);                                                                  /* slots 24-26: no caller found */
    void (*CustShipSlot25)(void);
    void (*CustShipSlot26)(void);
    bool32 (*CustShipHaltCommand)(struct Ship *ship);                                              /* slot 27, CAT new */
    void (*CustShipSlot28)(void);                                                                  /* slots 28-33: no caller found */
    void (*CustShipSlot29)(void);
    void (*CustShipSlot30)(void);
    void (*CustShipSlot31)(void);
    void (*CustShipSlot32)(void);
    void (*CustShipSlot33)(void);
    bool32 (*CustShipFormationCommand)(struct Ship *ship, sdword formation);                       /* slot 34, CAT new */
} CustShipHeader;

// CAT: ShipStaticInfo is 0x458 bytes in Cataclysm (HW1 0x370): ShipStaticInfos 0x00b3b420 is 145 x 0x458. The
//      layout below reproduces the exe's offsets on an ILP32 target (the /*+0x...*/ comments, checked by the static
//      asserts after the struct). Sources: ShipStaticScriptTable 0x008b2428 (every scripted field), InitStatShipInfo
//      0x0053bab0 (defaults and fix-ups), CopyPointersFromRealShipStaticInfo 0x004ffc60 (custstatinfo +0x194,
//      dockOverideInfo +0x3F4, ShipXDocksAtShipY/LaunchShipXFromShipY +0x400/+0x404), scriptSetGunStatics 0x00526790
//      (+0x3EC), scriptSetDockStatics 0x005270c0 (+0x3F0), CET game/Game/spaceobj.h. Fields marked "unknown" are
//      present in the exe but never named; they are kept as padding so every known field sits at its exe offset.
//      HW1's tractorEmitterStatic and SO_CLOOGE_SCALE scaleFactor have no slot in the exe and are gone.
typedef struct ShipStaticInfo
{
    union {
        StaticInfo common;
        StaticInfoHealth health;
        StaticInfoHealthGuidance guidance;
        StaticInfoShipOrDerelict shipOrDerelict;
    };                                          /*+0x000, 0x104 bytes */

    //TODO(type safety, 2026-04-24): Merge into StaticInfoShipOrDerelict
    NAVLightStaticInfo *navlightStaticInfo;     /*+0x104*/
/*ship specific stuff below */
    CustShipHeader custshipheader;              /*+0x108, 0x8C bytes */
    void *custstatinfo;                         /*+0x194*/
    real32 bulletRangeSquared[NUM_TACTICS_TYPES];/*+0x198*/
    real32 bulletRange[NUM_TACTICS_TYPES];      /*+0x1A4*/
    real32 minBulletRange[NUM_TACTICS_TYPES];   /*+0x1B0*/
    real32 blastRadiusShockWave;                /*+0x1BC*/
    real32 blastRadiusDamage;                   /*+0x1C0*/
    real32 passiveRetaliateZone;                /*+0x1C4*/
    real32 collSideModifiers[NUM_TRANS_DEGOFFREEDOM];/*+0x1C8*/

    ShipType shiptype;                          /*+0x1E0*/
    ShipRace shiprace;                          /*+0x1E4*/
    ShipClass shipclass;                        /*+0x1E8*/
    udword catTypeFlags;                        /*+0x1EC CAT: per-type category bits, cat_univInitShipTypeFlags 0x0053b8c0 (meaning of the bits unknown) */
    bool8 externalBuild;                        /*+0x1F0 CAT: key externalBuild (scriptSetBool8; CET declares bool32) */
    ubyte pad1F1[3];
    real32 forceFieldProtectionDistanceScale;   /*+0x1F4 CAT: key, default -1.0, <= 1.0 after load -> 2.0 [audit: was "<= 0"] */
    real32 upgradeTimeModifier;                 /*+0x1F8 CAT: key, default 1.0 */

    bool8 canBuildShips;                        /*+0x1FC*/
    bool8 canReceiveShips;
    bool8 canReceiveResources;
    bool8 canReceiveShipsPermanently;
    bool8 canReceiveShipsForRetire;             /*+0x200*/
    bool8 canReceiveSomething;
    bool8 canBuildBigShips;
    bool8 canTargetMultipleTargets;
    bool8 rotateToRetaliate;                    /*+0x204*/
    bool8 canReceiveTheseShips[4];              /*+0x205*/

    bool32 passiveAttackPenaltyExempt;          /*+0x20C*/

    bool8 specialActivateIsContinuous;          /*+0x210*/
    bool8 canSpecialBandBoxFriendlies;          /*+0x211*/
    bool8 unknownFlag212;                       /*+0x212 CAT: unknown, InitStatShipInfo sets 1 */
    bool8 unknownFlag213;                       /*+0x213 CAT: unknown, InitStatShipInfo sets 0 */
    bool8 canSingleClickSpecialActivate;        /*+0x214*/

    sword maxDockableFighters;  // maximum fighters that can dock at this ship   /*+0x216*/
    sword maxDockableCorvettes; // maximum corvettes that can dock at this ship  /*+0x218*/

    sdword canHandleNumShipsDocking;            /*+0x21C*/
    sdword canHandleNumShipsDepositingRU;       /*+0x220*/

    bool32 repairBeamCapable;                   /*+0x224*/
    real32 healthPerSecond;
    real32 CapitalDistanceRepairStart2;
    real32 CapitalDistanceRepairStart;
    real32 AngleDotProdThreshold;

    sdword buildCost;                           /*+0x238*/
    sdword buildTime;                           /*+0x23C*/
    real32 fowRadius;                           /*+0x240 CAT: key fowRadius */
    sdword svManeuverability;                   /*+0x244*/
    sdword svCoverage;
    sdword svFirePower;
    real32 RKOEatMeTime;                        /*+0x250 CAT: key RKOEatMeTime */

    sdword resourcesAtOneTime;                  /*+0x254*/
    sdword maxresources;
    sdword harvestRate;
    sdword harvestAmount;

    sdword groupSize;                           /*+0x264*/

    // CAT: the fuel fields keep their slots; their keys (maxfuel, lowfuelpoint, fuelburnrate, attackfuelburnrate,
    //      pumpFuelRate) are gone from ShipStaticScriptTable 0x008b2428, so they stay 0 (no fuel in Cataclysm)
    real32 maxfuel;                             /*+0x268*/
    real32 lowfuelpoint;
    real32 fuelburnrate;
    real32 attackfuelburnrate;

    real32 repairTime;          // self-repair for capital ships   /*+0x278*/
    real32 repairDamage;
    real32 repairCombatTime;    // self-repair for capital ships in combat
    real32 repairCombatDamage;

    real32 pumpFuelRate;        // rate at which ship can refuel other ships   /*+0x288*/
    real32 repairOtherShipRate; // rate at which ship can repair other ships
    real32 repairResourceCollectorRate; //specifically for when a ship is repairing a resource collector.  0.0f deafults to previous value
    real32 clearanceDistance;   // distance which other docking ship should fly away for clearance
    udword clearanceDirection;  // direction which other ship should fly away for clearance (0..7)

    real32 dockShipRange;       // distance to ship to get for docking, squared   /*+0x29C*/

    real32 formationPaddingModifier;                                               /*+0x2A0*/
    bool32 cannotForceAttackIfOwnShip;    //variable..if set to TRUE player cannot force attack this ship IF it is their own ship
    bool32 shipIsCapital;                 //Set to TRUE is ship is a TRUE capital ship (class defs are unreliable)
    bool32 cantMoveAndAttack;             //if set, SHIP explicitly cannot move and attack...will be given a special move order.
    // CAT: the engine nozzles are at +0x2B0 and are followed by a per-LOD mesh-object index of each nozzle
    //      (mexSetEngineNozzle 0x004b7c20 writes +0x2B0 + index * 0xC and +0x2E0 + level * 4 + index;
    //      trailGetNozzleOffset 0x005a4cc0 reads both). M2-R2 had placed the nozzles at +0x2C8 by position.
    vector engineNozzleOffset[MAX_NUM_TRAILS];      //offset of engine in object co-ordinates   /*+0x2B0*/
    ubyte engineNozzleObject[LOD_NumberLevels][MAX_NUM_TRAILS];   /*+0x2E0 CAT: polygon object index, 0xFF = none */
    trailstatic* trailStatic[MAX_NUM_TRAILS];   /*+0x2F8*/
    udword multipleEngines;                     /*+0x308*/

    real32 scaleCap;            //minimum sizing cap, which prevents a ship from getting too small   /*+0x30C key N-LIPS*/

    sdword hierarchySize;       //size of the ship hierarchy     /*+0x310*/
    madstatic *madStatic;       //mesh animation binding and animation data   /*+0x314*/
    real32 madGunOpenDamagedHealthThreshold;    /*+0x318*/
    real32 madWingOpenDamagedHealthThreshold;   /*+0x31C*/

    // Alex's repair droid stuff...
    udword nRepairDroids;                       /*+0x320 CAT: key repairDroids (HW1 had the field commented out) */

    // Keith's engine trail stuff...
    udword trailStyle[MAX_NUM_TRAILS];          /*+0x324*/
    real32 trailWidth[MAX_NUM_TRAILS];
    real32 trailHeight[MAX_NUM_TRAILS];
    real32 trailAngle[MAX_NUM_TRAILS];
    real32 trailRibbonAdjust[MAX_NUM_TRAILS];
    real32 trailLength[MAX_NUM_TRAILS];
    real32 trailScaleCap[MAX_NUM_TRAILS];
    real32 trailSpriteRadius[MAX_NUM_TRAILS];
    real32 trailSpriteOffset[MAX_NUM_TRAILS];

    real32 minimumZoomDistance;                 /*+0x3B4*/
    real32 renderlistFade, renderlistLimitSqr;

    real32 dockLightNear, dockLightFar;         /*+0x3C0*/
    color  dockLightColor;                      /*+0x3C8*/

    color  hyperspaceColor;                     /*+0x3CC*/

    real32 hyperspaceBoxScale;                  /*+0x3D0 CAT: key, default 1.0 */
    udword leechRUsPerSecond;                   /*+0x3D4 CAT: key (scriptSetUdwordCB), default 1 */
    real32 leechHPsPerSecond;                   /*+0x3D8 CAT: key, default 20.0 */
    real32 beastAttackCascadeCost;              /*+0x3DC CAT: key, default 1.0 */
    sdword beastAttacksRequired;                /*+0x3E0 CAT: key, default -1 */
    ShipType beastAttackConvertTo;              /*+0x3E4 CAT: key (cat_scriptSetBeastAttackConvertToCB), default bWorker */

    real32 tacticalSelectionScale;              /*+0x3E8 key Overlay_Selection_Scalar */

    GunStaticInfo *gunStaticInfo;               /*+0x3EC*/
    DockStaticInfo *dockStaticInfo;             /*+0x3F0*/
    DockOverideInfo *dockOverideInfo;           /*+0x3F4*/
    ResNozzleStatic *resNozzleStatic;           /*+0x3F8*/
    RepairNozzleStatic *repairNozzleStatic;     /*+0x3FC*/
    bool32 (*ShipXDocksAtShipY) (struct CommandToDo *docktodo,struct Ship *ship,struct Ship *dockwith);  /*+0x400*/
    bool32 (*LaunchShipXFromShipY) (struct Ship *ship,struct Ship *dockwith);                           /*+0x404*/

    bool32 bUseRayTriangleIntersectionForHitLocation;   /*+0x408 CAT: key, default TRUE */
    real32 linkDistance;                        /*+0x40C CAT: key, default 0 */
    udword supportUnits;                        /*+0x410 CAT: key */
    udword providesSU;                          /*+0x414 CAT: key */
    real32 RamDamage;                           /*+0x418 CAT: key */
    real32 cuttingBeamDamagePerSecond;          /*+0x41C CAT: key */
    real32 cuttingBeamUpgradedDamagePerSecond;  /*+0x420 CAT: key */
    real32 RamPushTime;                         /*+0x424 CAT: key */
    real32 RamWaitAfterPushTime;                /*+0x428 CAT: key */
    udword unknown42C;                          /*+0x42C CAT: unknown */
    real32 TimeShockedByEMP;                    /*+0x430 CAT: key */
    real32 EMP_Range;                           /*+0x434 CAT: key */
    real32 EMP_Max_Combine_Distance;            /*+0x438 CAT: key */
    bool32 UpgradeByDocking;                    /*+0x43C CAT: key, default FALSE */
    real32 pilotViewOffsetX;                    /*+0x440 CAT: key, default 0 */
    real32 pilotViewOffsetY;                    /*+0x444 CAT: key, default 0 */
    real32 pilotViewOffsetZ;                    /*+0x448 CAT: key, default 0 */
    real32 maxhealthBase;                       /*+0x44C CAT: maxhealth after GLOBAL_SHIP_HEALTH_MODIFIER, before the
                                                   single-player difficulty factor (universeStaticInit 0x0053d2a0,
                                                   cat_univSetShipMaxHealth 0x0053c4a0) */
    udword unknown450;                          /*+0x450 CAT: unknown */
    udword RKOGetsMissiles;                     /*+0x454 CAT: key (InitStatShipInfo clears its low byte only) */
} ShipStaticInfo;

/*-----------------------------------------------------------------------------
    CAT: layout pins for the three Cataclysm ship-static structs.
    On an ILP32 target (the exe's) the sizes and the key offsets must be the exe's: ShipStaticInfo 0x458
    (ShipStaticInfos 0x00b3b420, 145 x 0x458), GunStatic 0x11C (scriptSetGunStatics 0x00526790 stride),
    CustShipHeader 0x8C (InitStatShipInfo 0x0053bab0 copies 0x23 dwords). tools/m2_layout32.sh compiles this header
    with -m32 to check that branch. On LP64 the pointers are 8 bytes, so the sizes are pinned to this layout's
    LP64 values instead: a change to either branch is a deliberate layout change.
-----------------------------------------------------------------------------*/
#define CAT_ASSERT_OFFSET(type, field, off) \
    _Static_assert(offsetof(type, field) == (off), #type "." #field " is at " #off " in cataclysm.exe")
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(GunStatic) == 0x11C, "GunStatic is 0x11C bytes in cataclysm.exe");
_Static_assert(sizeof(CustShipHeader) == 0x8C, "CustShipHeader is 0x8C bytes in cataclysm.exe");
_Static_assert(sizeof(ShipStaticInfo) == 0x458, "ShipStaticInfo is 0x458 bytes in cataclysm.exe");
_Static_assert(sizeof(StaticInfo) == 0xA4, "StaticHeader is 0xA4 bytes in cataclysm.exe");
CAT_ASSERT_OFFSET(GunStatic, startUpgradeLevel, 0x0C);
CAT_ASSERT_OFFSET(GunStatic, gunSize, 0x14);
CAT_ASSERT_OFFSET(GunStatic, damageMult, 0x18);
CAT_ASSERT_OFFSET(GunStatic, cosminAngleFromNorm, 0x3C);
CAT_ASSERT_OFFSET(GunStatic, minturnangle, 0x8C);
CAT_ASSERT_OFFSET(GunStatic, maxMissiles, 0xAC);
CAT_ASSERT_OFFSET(GunStatic, baseGunDamageLo, 0xCC);
CAT_ASSERT_OFFSET(GunStatic, bulletlength, 0xD4);
CAT_ASSERT_OFFSET(GunStatic, burstWaitTime, 0xF0);
CAT_ASSERT_OFFSET(GunStatic, bulletlifetime2, 0xF4);
CAT_ASSERT_OFFSET(GunStatic, firetime2, 0x100);
CAT_ASSERT_OFFSET(GunStatic, barrelLength, 0x104);
CAT_ASSERT_OFFSET(GunStatic, slaveDriver, 0x118);
CAT_ASSERT_OFFSET(CustShipHeader, CustShipStaticInit, 0x04);
CAT_ASSERT_OFFSET(CustShipHeader, CustShipInit, 0x0C);
CAT_ASSERT_OFFSET(CustShipHeader, CustShipAttackPassive, 0x18);
CAT_ASSERT_OFFSET(CustShipHeader, CustShipDied, 0x3C);
CAT_ASSERT_OFFSET(CustShipHeader, CustShipAttack, 0x54);
CAT_ASSERT_OFFSET(CustShipHeader, CustShipHaltCommand, 0x6C);
CAT_ASSERT_OFFSET(CustShipHeader, CustShipFormationCommand, 0x88);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.collspheresize, 0x4C);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.collsphereoffset, 0x54);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.collrectoffset, 0x60);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.collrectmid, 0x6C);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.preciseSelection, 0x88);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.collBoxOffsetZ, 0xA0);
CAT_ASSERT_OFFSET(ShipStaticInfo, health.maxhealth, 0xA4);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.turnspeedstat[TURN_ROLL], 0xEC);
CAT_ASSERT_OFFSET(ShipStaticInfo, shipOrDerelict.teamColor, 0xF0);
CAT_ASSERT_OFFSET(ShipStaticInfo, shipOrDerelict.sinbank, 0xF8);
CAT_ASSERT_OFFSET(ShipStaticInfo, navlightStaticInfo, 0x104);
CAT_ASSERT_OFFSET(ShipStaticInfo, custshipheader, 0x108);
CAT_ASSERT_OFFSET(ShipStaticInfo, custshipheader.CustShipAttack, 0x15C);
// CAT (M2-R3 systemic headers): CustShipFire is slot 5 at staticinfo+0x11C (the Fire callers GenericInterceptorAttack,
//      MultiBeamFrigateAttack, doKamikazeAttack, nisFire call [statinfo+0x11C]); StaticCollInfo starts at +0x34 and its
//      lengths sit 3 dwords later than HW1's because of collrectmid (GetCollSizeInDirection and mexSetCollisionInfo
//      0x004b7880 read/write uplength +0x78, rightlength +0x7C, forwardlength +0x80)
CAT_ASSERT_OFFSET(ShipStaticInfo, custshipheader.CustShipFire, 0x11C);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo, 0x34);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.uplength, 0x78);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.rightlength, 0x7C);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.forwardlength, 0x80);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.diagonallength, 0x84);
CAT_ASSERT_OFFSET(ShipStaticInfo, custstatinfo, 0x194);
CAT_ASSERT_OFFSET(ShipStaticInfo, bulletRangeSquared, 0x198);
CAT_ASSERT_OFFSET(ShipStaticInfo, collSideModifiers, 0x1C8);
CAT_ASSERT_OFFSET(ShipStaticInfo, shiptype, 0x1E0);
CAT_ASSERT_OFFSET(ShipStaticInfo, shipclass, 0x1E8);
CAT_ASSERT_OFFSET(ShipStaticInfo, externalBuild, 0x1F0);
CAT_ASSERT_OFFSET(ShipStaticInfo, upgradeTimeModifier, 0x1F8);
CAT_ASSERT_OFFSET(ShipStaticInfo, canBuildShips, 0x1FC);
CAT_ASSERT_OFFSET(ShipStaticInfo, canReceiveTheseShips, 0x205);
CAT_ASSERT_OFFSET(ShipStaticInfo, passiveAttackPenaltyExempt, 0x20C);
CAT_ASSERT_OFFSET(ShipStaticInfo, canSingleClickSpecialActivate, 0x214);
CAT_ASSERT_OFFSET(ShipStaticInfo, maxDockableFighters, 0x216);
CAT_ASSERT_OFFSET(ShipStaticInfo, canHandleNumShipsDocking, 0x21C);
CAT_ASSERT_OFFSET(ShipStaticInfo, repairBeamCapable, 0x224);
CAT_ASSERT_OFFSET(ShipStaticInfo, buildCost, 0x238);
CAT_ASSERT_OFFSET(ShipStaticInfo, fowRadius, 0x240);
CAT_ASSERT_OFFSET(ShipStaticInfo, RKOEatMeTime, 0x250);
CAT_ASSERT_OFFSET(ShipStaticInfo, groupSize, 0x264);
CAT_ASSERT_OFFSET(ShipStaticInfo, maxfuel, 0x268);
CAT_ASSERT_OFFSET(ShipStaticInfo, repairTime, 0x278);
CAT_ASSERT_OFFSET(ShipStaticInfo, pumpFuelRate, 0x288);
CAT_ASSERT_OFFSET(ShipStaticInfo, dockShipRange, 0x29C);
CAT_ASSERT_OFFSET(ShipStaticInfo, cantMoveAndAttack, 0x2AC);
CAT_ASSERT_OFFSET(ShipStaticInfo, engineNozzleOffset, 0x2B0);
CAT_ASSERT_OFFSET(ShipStaticInfo, engineNozzleObject, 0x2E0);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailStatic, 0x2F8);
CAT_ASSERT_OFFSET(ShipStaticInfo, multipleEngines, 0x308);
CAT_ASSERT_OFFSET(ShipStaticInfo, scaleCap, 0x30C);
CAT_ASSERT_OFFSET(ShipStaticInfo, madStatic, 0x314);
CAT_ASSERT_OFFSET(ShipStaticInfo, nRepairDroids, 0x320);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailStyle, 0x324);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailSpriteOffset, 0x3A4);
CAT_ASSERT_OFFSET(ShipStaticInfo, minimumZoomDistance, 0x3B4);
CAT_ASSERT_OFFSET(ShipStaticInfo, hyperspaceColor, 0x3CC);
CAT_ASSERT_OFFSET(ShipStaticInfo, hyperspaceBoxScale, 0x3D0);
CAT_ASSERT_OFFSET(ShipStaticInfo, beastAttackConvertTo, 0x3E4);
CAT_ASSERT_OFFSET(ShipStaticInfo, tacticalSelectionScale, 0x3E8);
CAT_ASSERT_OFFSET(ShipStaticInfo, gunStaticInfo, 0x3EC);
CAT_ASSERT_OFFSET(ShipStaticInfo, dockStaticInfo, 0x3F0);
CAT_ASSERT_OFFSET(ShipStaticInfo, dockOverideInfo, 0x3F4);
CAT_ASSERT_OFFSET(ShipStaticInfo, resNozzleStatic, 0x3F8);
CAT_ASSERT_OFFSET(ShipStaticInfo, repairNozzleStatic, 0x3FC);
CAT_ASSERT_OFFSET(ShipStaticInfo, ShipXDocksAtShipY, 0x400);
CAT_ASSERT_OFFSET(ShipStaticInfo, bUseRayTriangleIntersectionForHitLocation, 0x408);
CAT_ASSERT_OFFSET(ShipStaticInfo, linkDistance, 0x40C);
CAT_ASSERT_OFFSET(ShipStaticInfo, RamDamage, 0x418);
CAT_ASSERT_OFFSET(ShipStaticInfo, TimeShockedByEMP, 0x430);
CAT_ASSERT_OFFSET(ShipStaticInfo, UpgradeByDocking, 0x43C);
CAT_ASSERT_OFFSET(ShipStaticInfo, pilotViewOffsetX, 0x440);
CAT_ASSERT_OFFSET(ShipStaticInfo, RKOGetsMissiles, 0x454);
// CAT: every other key of ShipStaticScriptTable 0x008b2428 at its exe offset (generated from the exe table)
CAT_ASSERT_OFFSET(ShipStaticInfo, common.LOD, 0x000);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.pMexData, 0x004);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.mass, 0x008);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.momentOfInertiaX, 0x00C);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.momentOfInertiaY, 0x010);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.momentOfInertiaZ, 0x014);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.collBoxScaleRight, 0x094);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.collBoxScaleUp, 0x090);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.collBoxScaleForward, 0x08C);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.collBoxOffsetX, 0x098);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.collBoxOffsetY, 0x09C);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.immobile, 0x031);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.rightOfWay, 0x032);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.approxcollmodifier, 0x040);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.staticCollInfo.avoidcollmodifier, 0x044);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.maxvelocity, 0x028);
CAT_ASSERT_OFFSET(ShipStaticInfo, common.maxrot, 0x02C);
CAT_ASSERT_OFFSET(ShipStaticInfo, bulletRange[0], 0x1A4);
CAT_ASSERT_OFFSET(ShipStaticInfo, bulletRange[1], 0x1A8);
CAT_ASSERT_OFFSET(ShipStaticInfo, bulletRange[2], 0x1AC);
CAT_ASSERT_OFFSET(ShipStaticInfo, minBulletRange[0], 0x1B0);
CAT_ASSERT_OFFSET(ShipStaticInfo, minBulletRange[1], 0x1B4);
CAT_ASSERT_OFFSET(ShipStaticInfo, minBulletRange[2], 0x1B8);
CAT_ASSERT_OFFSET(ShipStaticInfo, blastRadiusShockWave, 0x1BC);
CAT_ASSERT_OFFSET(ShipStaticInfo, blastRadiusDamage, 0x1C0);
CAT_ASSERT_OFFSET(ShipStaticInfo, passiveRetaliateZone, 0x1C4);
CAT_ASSERT_OFFSET(ShipStaticInfo, rotateToRetaliate, 0x204);
CAT_ASSERT_OFFSET(ShipStaticInfo, canTargetMultipleTargets, 0x203);
CAT_ASSERT_OFFSET(ShipStaticInfo, canReceiveShips, 0x1FD);
CAT_ASSERT_OFFSET(ShipStaticInfo, canReceiveResources, 0x1FE);
CAT_ASSERT_OFFSET(ShipStaticInfo, canReceiveShipsPermanently, 0x1FF);
CAT_ASSERT_OFFSET(ShipStaticInfo, canReceiveShipsForRetire, 0x200);
CAT_ASSERT_OFFSET(ShipStaticInfo, canBuildBigShips, 0x202);
CAT_ASSERT_OFFSET(ShipStaticInfo, canReceiveTheseShips[THISSHIPIS_FIGHTER], 0x205);
CAT_ASSERT_OFFSET(ShipStaticInfo, canReceiveTheseShips[THISSHIPIS_CORVETTE], 0x206);
CAT_ASSERT_OFFSET(ShipStaticInfo, canReceiveTheseShips[THISSHIPIS_RESOURCER], 0x207);
CAT_ASSERT_OFFSET(ShipStaticInfo, canReceiveTheseShips[THISSHIPIS_OTHERNONCAPITALSHIP], 0x208);
CAT_ASSERT_OFFSET(ShipStaticInfo, canHandleNumShipsDepositingRU, 0x220);
CAT_ASSERT_OFFSET(ShipStaticInfo, svManeuverability, 0x244);
CAT_ASSERT_OFFSET(ShipStaticInfo, svCoverage, 0x248);
CAT_ASSERT_OFFSET(ShipStaticInfo, svFirePower, 0x24C);
CAT_ASSERT_OFFSET(ShipStaticInfo, maxDockableCorvettes, 0x218);
CAT_ASSERT_OFFSET(ShipStaticInfo, specialActivateIsContinuous, 0x210);
CAT_ASSERT_OFFSET(ShipStaticInfo, canSpecialBandBoxFriendlies, 0x211);
CAT_ASSERT_OFFSET(ShipStaticInfo, supportUnits, 0x410);
CAT_ASSERT_OFFSET(ShipStaticInfo, providesSU, 0x414);
CAT_ASSERT_OFFSET(ShipStaticInfo, EMP_Range, 0x434);
CAT_ASSERT_OFFSET(ShipStaticInfo, EMP_Max_Combine_Distance, 0x438);
CAT_ASSERT_OFFSET(ShipStaticInfo, cuttingBeamDamagePerSecond, 0x41C);
CAT_ASSERT_OFFSET(ShipStaticInfo, cuttingBeamUpgradedDamagePerSecond, 0x420);
CAT_ASSERT_OFFSET(ShipStaticInfo, RamPushTime, 0x424);
CAT_ASSERT_OFFSET(ShipStaticInfo, RamWaitAfterPushTime, 0x428);
CAT_ASSERT_OFFSET(ShipStaticInfo, pilotViewOffsetY, 0x444);
CAT_ASSERT_OFFSET(ShipStaticInfo, pilotViewOffsetZ, 0x448);
CAT_ASSERT_OFFSET(ShipStaticInfo, buildTime, 0x23C);
CAT_ASSERT_OFFSET(ShipStaticInfo, resourcesAtOneTime, 0x254);
CAT_ASSERT_OFFSET(ShipStaticInfo, maxresources, 0x258);
CAT_ASSERT_OFFSET(ShipStaticInfo, harvestAmount, 0x260);
CAT_ASSERT_OFFSET(ShipStaticInfo, harvestRate, 0x25C);
CAT_ASSERT_OFFSET(ShipStaticInfo, health.minCollDamage, 0x0AC);
CAT_ASSERT_OFFSET(ShipStaticInfo, health.maxCollDamage, 0x0B0);
CAT_ASSERT_OFFSET(ShipStaticInfo, repairDamage, 0x27C);
CAT_ASSERT_OFFSET(ShipStaticInfo, repairCombatTime, 0x280);
CAT_ASSERT_OFFSET(ShipStaticInfo, repairCombatDamage, 0x284);
CAT_ASSERT_OFFSET(ShipStaticInfo, repairOtherShipRate, 0x28C);
CAT_ASSERT_OFFSET(ShipStaticInfo, repairResourceCollectorRate, 0x290);
CAT_ASSERT_OFFSET(ShipStaticInfo, clearanceDistance, 0x294);
CAT_ASSERT_OFFSET(ShipStaticInfo, clearanceDirection, 0x298);
CAT_ASSERT_OFFSET(ShipStaticInfo, formationPaddingModifier, 0x2A0);
CAT_ASSERT_OFFSET(ShipStaticInfo, cannotForceAttackIfOwnShip, 0x2A4);
CAT_ASSERT_OFFSET(ShipStaticInfo, shipIsCapital, 0x2A8);
CAT_ASSERT_OFFSET(ShipStaticInfo, healthPerSecond, 0x228);
CAT_ASSERT_OFFSET(ShipStaticInfo, CapitalDistanceRepairStart2, 0x22C);
CAT_ASSERT_OFFSET(ShipStaticInfo, CapitalDistanceRepairStart, 0x230);
CAT_ASSERT_OFFSET(ShipStaticInfo, AngleDotProdThreshold, 0x234);
CAT_ASSERT_OFFSET(ShipStaticInfo, collSideModifiers[TRANS_UP], 0x1C8);
CAT_ASSERT_OFFSET(ShipStaticInfo, collSideModifiers[TRANS_DOWN], 0x1CC);
CAT_ASSERT_OFFSET(ShipStaticInfo, collSideModifiers[TRANS_RIGHT], 0x1D0);
CAT_ASSERT_OFFSET(ShipStaticInfo, collSideModifiers[TRANS_LEFT], 0x1D4);
CAT_ASSERT_OFFSET(ShipStaticInfo, collSideModifiers[TRANS_FORWARD], 0x1D8);
CAT_ASSERT_OFFSET(ShipStaticInfo, collSideModifiers[TRANS_BACKWARD], 0x1DC);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.thruststrengthstat[TRANS_UP], 0x0B4);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.thruststrengthstat[TRANS_DOWN], 0x0B8);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.thruststrengthstat[TRANS_RIGHT], 0x0BC);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.thruststrengthstat[TRANS_LEFT], 0x0C0);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.thruststrengthstat[TRANS_FORWARD], 0x0C4);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.thruststrengthstat[TRANS_BACKWARD], 0x0C8);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.rotstrengthstat[ROT_YAWLEFT], 0x0CC);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.rotstrengthstat[ROT_YAWRIGHT], 0x0D0);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.rotstrengthstat[ROT_PITCHUP], 0x0D4);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.rotstrengthstat[ROT_PITCHDOWN], 0x0D8);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.rotstrengthstat[ROT_ROLLRIGHT], 0x0DC);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.rotstrengthstat[ROT_ROLLLEFT], 0x0E0);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.turnspeedstat[TURN_YAW], 0x0E4);
CAT_ASSERT_OFFSET(ShipStaticInfo, guidance.turnspeedstat[TURN_PITCH], 0x0E8);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailStyle[0], 0x324);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailWidth[0], 0x334);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailHeight[0], 0x344);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailAngle[0], 0x354);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailRibbonAdjust[0], 0x364);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailLength[0], 0x374);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailScaleCap[0], 0x384);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailStyle[1], 0x328);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailWidth[1], 0x338);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailHeight[1], 0x348);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailAngle[1], 0x358);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailRibbonAdjust[1], 0x368);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailLength[1], 0x378);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailScaleCap[1], 0x388);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailStyle[2], 0x32C);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailWidth[2], 0x33C);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailHeight[2], 0x34C);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailAngle[2], 0x35C);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailRibbonAdjust[2], 0x36C);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailLength[2], 0x37C);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailScaleCap[2], 0x38C);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailStyle[3], 0x330);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailWidth[3], 0x340);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailHeight[3], 0x350);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailAngle[3], 0x360);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailRibbonAdjust[3], 0x370);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailLength[3], 0x380);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailScaleCap[3], 0x390);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailSpriteRadius[0], 0x394);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailSpriteRadius[1], 0x398);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailSpriteRadius[2], 0x39C);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailSpriteRadius[3], 0x3A0);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailSpriteOffset[0], 0x3A4);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailSpriteOffset[1], 0x3A8);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailSpriteOffset[2], 0x3AC);
CAT_ASSERT_OFFSET(ShipStaticInfo, trailSpriteOffset[3], 0x3B0);
CAT_ASSERT_OFFSET(ShipStaticInfo, renderlistFade, 0x3B8);
CAT_ASSERT_OFFSET(ShipStaticInfo, renderlistLimitSqr, 0x3BC);
CAT_ASSERT_OFFSET(ShipStaticInfo, dockLightNear, 0x3C0);
CAT_ASSERT_OFFSET(ShipStaticInfo, dockLightFar, 0x3C4);
CAT_ASSERT_OFFSET(ShipStaticInfo, dockLightColor, 0x3C8);
CAT_ASSERT_OFFSET(ShipStaticInfo, madGunOpenDamagedHealthThreshold, 0x318);
CAT_ASSERT_OFFSET(ShipStaticInfo, madWingOpenDamagedHealthThreshold, 0x31C);
CAT_ASSERT_OFFSET(ShipStaticInfo, leechRUsPerSecond, 0x3D4);
CAT_ASSERT_OFFSET(ShipStaticInfo, leechHPsPerSecond, 0x3D8);
CAT_ASSERT_OFFSET(ShipStaticInfo, beastAttackCascadeCost, 0x3DC);
CAT_ASSERT_OFFSET(ShipStaticInfo, beastAttacksRequired, 0x3E0);
CAT_ASSERT_OFFSET(ShipStaticInfo, forceFieldProtectionDistanceScale, 0x1F4);
// CAT: DockStaticInfo 0x50-byte header and 0x4C-byte points (scriptSetDockStatics 0x005270c0: 0x4C * n + 0x50,
//      n at +0x48, firstOfType at +0, numOfType at +0x23; cat_dockFindDockStaticPoint 0x00463d00: name at +0x38)
_Static_assert(sizeof(DockStaticPoint) == 0x4C, "DockStaticPoint is 0x4C bytes in cataclysm.exe");
CAT_ASSERT_OFFSET(DockStaticPoint, headingdirection, 0x30);
CAT_ASSERT_OFFSET(DockStaticPoint, name, 0x38);
CAT_ASSERT_OFFSET(DockStaticInfo, numOfType, 0x23);
CAT_ASSERT_OFFSET(DockStaticInfo, numDockPoints, 0x48);
CAT_ASSERT_OFFSET(DockStaticInfo, centerDockPoint, 0x4C);
CAT_ASSERT_OFFSET(DockStaticInfo, dockstaticpoints, 0x50);
// CAT: NAVLightStatic is 0x48 bytes (scriptSetNAVLightStatics 0x00526dc0 stride; mexGetNAVLightStaticInfo 0x004b7680:
//      name at +0x28, per-LOD object bytes at +0x40)
_Static_assert(sizeof(NAVLightStatic) == 0x48, "NAVLightStatic is 0x48 bytes in cataclysm.exe");
CAT_ASSERT_OFFSET(NAVLightStatic, name, 0x28);
CAT_ASSERT_OFFSET(NAVLightStatic, navlightObject, 0x40);
#else
_Static_assert(sizeof(void *) == 8, "the non-ILP32 pins below assume 8-byte pointers");
_Static_assert(sizeof(GunStatic) == 0x11C, "GunStatic has no pointers: 0x11C bytes on every target");
_Static_assert(sizeof(CustShipHeader) == 0x118, "CustShipHeader: 4 + 4 pad + 34 x 8 on LP64/LLP64");
_Static_assert(sizeof(ShipStaticInfo) == 0x540, "ShipStaticInfo: this layout with 8-byte pointers");
#endif

typedef struct
{
    union {
        StaticInfo common;
        StaticInfoHealth _health;
    };

    sdword resourcevalue;
    udword harvestableByMultipleShips;
} ResourceStaticInfo;

typedef struct AsteroidStaticInfo     // CAT: tagged for StatScript.h
{
    union {
        StaticInfo common;
        StaticInfoHealth health;
        ResourceStaticInfo resource;
    };

    AsteroidType asteroidtype;      // Asteroid specific stuff from here on    /*+0xBC*/
    // CAT: Cataclysm's asteroids break into the pieces their .shp lists (NUMBER_OF_ASTEROID_PIECES, AsteroidBreakInfo;
    //      cat_scriptAsteroidBreakInfoLoad 0x00528190); the exe's AsteroidStaticInfo ends here (0xC8 bytes, the
    //      asteroidStaticInfos stride in universeStaticInit 0x0053d2a0)
    sdword numBreakPieces;                                                      /*+0xC0*/
    struct AsteroidBreakPiece *breakPieces;                                     /*+0xC4*/
    // CAT-DORMANT: HW1's shrink/break fields. No Cataclysm key sets them (the exe's asteroid key table 0x008b3008 has
    //      none), so they stay 0 and ResCollect.c's HW1 shrink/break paths never run; they go when stage 2B ports
    //      Cataclysm's asteroid breaking (owner: M2-R4 stage-2B combat/resources)
    bool8 asteroidCanShrink;
    bool8 asteroidCanBreak;
    bool8 pad2;
    bool8 pad3;
    real32 asteroidMinShrink;
    udword breakinto[NUM_ASTEROIDTYPES];
} AsteroidStaticInfo;

// CAT: one AsteroidBreakInfo line of an asteroid .shp, 0x44 bytes (cat_scriptAsteroidPieceParse 0x005280a0:
//      "%d %f %f %f %f %f %f %f %f %f %f" after ',' -> ' ' and '?' -> '0'). The orientation is RotX(a) * RotY(b) *
//      RotY(b) of the 4th and 5th values: the exe never uses the 6th value (two matMakeRotAboutY calls,
//      0x005280a0..0x0052818f). The meaning of the last four values belongs to the breaking code (stage 2B).
typedef struct AsteroidBreakPiece
{
    AsteroidType asteroidtype;      /*+0x00*/
    vector position;                /*+0x04*/
    matrix orientation;             /*+0x10*/
    real32 params[4];               /*+0x34*/
} AsteroidBreakPiece;

typedef struct
{
    union {
        StaticInfo common;
        StaticInfoHealth health;
        ResourceStaticInfo resource;
    };

    NebulaType nebulatype;
    bool8 nebulaCanShrink;
    bool8 pad[3];
    real32 nebulaMinShrink;
} NebulaStaticInfo;

typedef struct
{
    union {
        StaticInfo common;
        StaticInfoHealth health;
        ResourceStaticInfo resource;
    };

    GasCloudType gascloudtype;
    bool8 gascloudCanShrink;
    bool8 pad[3];
    real32 gascloudMinShrink;
} GasCloudStaticInfo;

typedef struct
{
    union {
        StaticInfo common;
        StaticInfoHealth health;
        ResourceStaticInfo resource;
    };

    DustCloudType dustcloudtype;
    bool8 dustcloudCanShrink;
    bool8 pad[3];
    real32 dustcloudMinShrink;
} DustCloudStaticInfo;

typedef struct
{
    union {
        StaticInfo common;
        StaticInfoHealth health;
        StaticInfoHealthGuidance guidance;
        StaticInfoShipOrDerelict shipOrDerelict;
    };

    //TODO(type safety, 2026-04-24): Merge into StaticInfoShipOrDerelict
    NAVLightStaticInfo *navlightStaticInfo;
/* Derelict specific below */
    DerelictType derelicttype;
    bool32    salvageable;                        //TRUE if derelict is to be targetable by salcap corvettes

    real32 scaleFactor;                         //only used for 'world rendering'
    bool32   worldRender;                         //TRUE for Angel moon, homeworld, and POO

    real32 minimumZoomDistance;
    real32 renderlistFade, renderlistLimitSqr;
} DerelictStaticInfo;

typedef struct
{
    union {
        StaticInfo common;
        StaticInfoHealth health;
        StaticInfoHealthGuidance guidance;
    };
    bool8 teamColor[MAX_MULTIPLAYER_PLAYERS];           //what color schemes this can be


/* missile specific below*/
    real32 mineRange;
    real32 mineRangeForced;
    real32 MINE_STOP_FRICTION;
    real32 MINE_RANGESQR;
    real32 MINE_RANGESQR_FORCED;
    real32 maxvelocity_FORCED;
    real32 scaleCap;                // CAT: mine key N-LIPS (mine key table 0x008b3b88, +0x110)

    //missile trail stuff
    vector engineNozzleOffset;
    trailstatic* trailStatic;

    real32 trailWidth;
    real32 trailHeight;
} MissileStaticInfo;

#if defined(__i386__) || defined(_M_IX86)
// CAT: the exe's layouts of the other statics (universeStaticInit 0x0053d2a0 strides: asteroids 200, derelicts 0x124,
//      missiles and mines 300; InitStatMissileInfo 0x0053b400 stores trailStatic at +0x120 and the trail size at
//      +0x124/+0x128; cat_scriptAsteroidBreakInfoLoad 0x00528190 reads the piece count at +0xC0 and stores the pieces
//      at +0xC4)
_Static_assert(offsetof(AsteroidStaticInfo, asteroidtype) == 0xBC, "AsteroidStaticInfo.asteroidtype is at 0xBC in cataclysm.exe");
_Static_assert(offsetof(AsteroidStaticInfo, breakPieces) == 0xC4, "AsteroidStaticInfo.breakPieces is at 0xC4 in cataclysm.exe");
_Static_assert(offsetof(AsteroidStaticInfo, asteroidCanShrink) == 0xC8, "AsteroidStaticInfo is 0xC8 bytes in cataclysm.exe before the dormant HW1 fields");
_Static_assert(sizeof(AsteroidBreakPiece) == 0x44, "AsteroidBreakPiece is 0x44 bytes in cataclysm.exe");
_Static_assert(sizeof(DerelictStaticInfo) == 0x124, "DerelictStaticInfo is 0x124 bytes in cataclysm.exe");
_Static_assert(sizeof(MissileStaticInfo) == 0x12C, "MissileStaticInfo is 0x12C bytes in cataclysm.exe");
_Static_assert(offsetof(MissileStaticInfo, scaleCap) == 0x110, "MissileStaticInfo.scaleCap is at 0x110 in cataclysm.exe");
_Static_assert(offsetof(MissileStaticInfo, trailStatic) == 0x120, "MissileStaticInfo.trailStatic is at 0x120 in cataclysm.exe");
#endif

/*=============================================================================
    Object Instance Information
=============================================================================*/
/*
typedef struct
{
    sdword nSides;          //3 or 4 sides
    vector p[4];            //corners;
}
precisepoly;
*/
typedef struct
{
    vector worldRectPos[8]; //collision rect in world space
    ubyte  corner[24];      //3 polys, 4 corners each, plus padding for run-over
//    precisepoly poly[5];        //there could be up to 5 polys
    sdword  nPolys;         //number of camera-facing sides of collision box (0-5)
}
PreciseSelection;

#define MOVING_LINEARLY_BIT         BIT0
#define MOVING_ROTATIONALLY_BIT     BIT1

#define SET_MOVING_LINEARLY(x)      bitSet((x), MOVING_LINEARLY_BIT)
#define SET_MOVING_ROTATIONALLY(x)  bitSet((x), MOVING_ROTATIONALLY_BIT)
#define SET_MOVING_IMMOBILE(x)      bitClear((x), MOVING_LINEARLY_BIT | MOVING_ROTATIONALLY_BIT)

#define IS_MOVING_LINEARLY(x)       bitTest((x), MOVING_LINEARLY_BIT)
#define IS_MOVING_ROTATIONALLY(x)   bitTest((x), MOVING_ROTATIONALLY_BIT)
#define IS_MOVING_IMMOBILE(x)       (!(IS_MOVING_LINEARLY(x) || IS_MOVING_ROTATIONALLY(x)))

typedef struct
{
    vector position;        // object position in universe
    vector velocity;        // velocity of object
    vector force;           // force acting on object
    bitflag16 isMoving;           // bit flag indicating how object is moving
    bool16 haventCalculatedDist;  // flag indicating haven't yet calculated distance
} PosInfo;

typedef struct
{
    vector collOffset;      // collision sphere offset from position in object coordinates.
    vector collPosition;    // object collision position in universe = position + collOffset
    vector rectpos[8];      // rectangle corner positions (in object coordinates)
    PreciseSelection *precise;//precise collision info; not all ships have this
    real32 selCircleX, selCircleY;// on-screen location of selection circle on-screen
    real32 selCircleRadius; // on-screen size of selection circle
    real32 selCircleDepth;  // depth of object in camera space
} CollInfo;

typedef struct
{
    matrix coordsys;        // matrix describing co-ordinate system of object
    vector rotspeed;        // rotational speed (rad/s)
    vector torque;          // torque acting on object
} RotInfo;

#define     SF_SLAVE    0x01
#define     SF_MASTER   0x02
#define     SF_DEAD     0x04
typedef struct
{
    udword flags;           //indicates status of Slavery
    struct Ship *Master;           //
    vector offset;          //ships position in MASTER space
    matrix coordsys;        //ships orientation in MASTER space
    LinkedList slaves;      //Defined only if a master (master not in list)
} SlaveInfo;

/*-----------------------------------------------------------------------------
Layout of co-ordinate system:

The ship heading, upvector, and rightvectors are stored in the coordsys
matrix as follows:

|   upx     rightx  headingx  |
|   upy     righty  headingy  |
|   upz     rightz  headingz  |
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
    SpaceObj - General, base object from which all objects are derived
-----------------------------------------------------------------------------*/

// CAT: Cataclysm's object flags (SpaceObj.flags, +0x14 on ILP32). HW1's bits 0-7 are kept; above them Cataclysm drops
//      one cloak flag, SOF_BitForSaleCheap and SOF_Slaveable, so the rest move down by 1, 2 and 3 bits, and three new
//      flags take the top bits. Evidence, per flag (exe functions that set/test the bit where the HW1 body uses the
//      name): Rotatable|Impactable|Selectable|Targetable = 0x102003 (univCreateShip 0x00542320 `| 0x102003`),
//      Rotatable|Impactable|Targetable|Resource = 0x104003 (univAddAsteroid), | NotBumpable = 0x184003
//      (univAddDustCloud/GasCloud/Nebula), BigObject 0x20000 (univCreateShip), Dead|DontCreateBlob = 0x1800
//      (bobListCreate), Dead 0x800 (univGetChecksum 0x00549f50 and 33 other `& 0x800` tests), Cloaked 0x400 (DetectShips
//      `& 0x400` where HW1 tests SOF_Cloaked; cat_cloakUpdate 0x004361c0 sets it), Hide 0x8000 and HideSave 0x8000000
//      (kasfHideShips 0x004977a0 / kasfUnhideShips), HideSave2 0x10000000 (univHideJustAboutEverything,
//      univUnhideJustAboutEverything 0x00547cd0), Crazy 0x200000 (aitrackForceSteadyShip and 15 other functions),
//      DontDrawTrails 0x400000 (univSetupShipForControl, processMilitaryParadeToDo), Disabled 0x800000
//      (aishipmineaquiretarget), Clamped 0x1000000 (clampObjToObj 0x004346e0 `| 0x1000000`), Hyperspace 0x2000000
//      (ShipAlreadyHyperspaceOut), AlwaysSortFront 0x4000000 (etgEffectCreate 0x00470990 for ESO_SortForward),
//      StaticInfoIsDynamic 0x40000 (SaveShip/LoadShip), ForceVisible 0x10000, Selectable 0x2000 (selSelectionClick),
//      AttachPosition/Coordsys/Velocity/NLips 0x8/0x10/0x20/0x40 and NISShip 0x80 unchanged (etgEffectCreate,
//      univCheckShipState). Census and method: docs/m2-r3-closeout-headers.md "headers".
#define SOF_Rotatable           0x00000001
#define SOF_Impactable          0x00000002      // objects such as asteroids, ships which
                                                // can impart a change in momentum
#define SOF_DontApplyPhysics    0x00000004      // don't apply physics to such things as
                                                // explosions
#define SOF_AttachPosition      0x00000008      // prevent 'torpedo effects'
#define SOF_AttachCoordsys      0x00000010      // ditto, but more so
#define SOF_AttachVelocity      0x00000020
#define SOF_AttachNLips         0x00000040      // for ETG N-Lips inherited from ships

#define SOF_NISShip             0x00000080      //subject to 'NIS physics'

// CAT: 0x100 is never set or tested on an object in the exe (HW1's cloak generator is gone); 0x200 marks a ship that
//      carries a Cataclysm cloak (cat_cloakInit 0x00435f10 sets it; cat_selStatusBarsDraw shows the cloak charge,
//      cat_shipIsCloakedFully 0x00436110 tests 0x200 && 0x400); 0x400 is the cloaked state. The cloak state machine
//      lives in the ship's cloak record (cat_cloakUpdate states 0-3), so HW1's Cloaking/DeCloaking/CloakGenField flags
//      have no bit: they are 0, which leaves the HW1 cloak code that still tests them inert (it is gone in the exe).
#define SOF_CloakGenField       0x00000000      // CAT: retired (HW1 0x100)
#define SOF_CatCloakDevice      0x00000200      // CAT: new: the ship has a Cataclysm cloak (name chosen here)
#define SOF_Cloaked             0x00000400      // CAT: was 0x200. object is cloaked for whatever reason
#define SOF_Cloaking            0x00000000      // CAT: retired (HW1 0x400)
#define SOF_DeCloaking          0x00000000      // CAT: retired (HW1 0x800)

#define SOF_Dead                0x00000800      // CAT: was 0x1000. Indicates object is dead, and cannot interact with selection/targeting in any way

#define SOF_Selected            0x00001000      // CAT: was 0x2000. ship selection shite
#define SOF_DontCreateBlob      0x00001000      // CAT: was 0x2000. this object's blob is fine as it is
// CAT: SOF_BitForSaleCheap (HW1 0x4000, unused) is gone

#define SOF_Selectable          0x00002000      // CAT: was 0x8000. Indicates object is selectable

#define SOF_Resource            0x00004000      // CAT: was 0x10000. Indicates object is a resource

#define SOF_Hide                0x00008000      // CAT: was 0x20000. Indicates object should be hidden (never drawn, and never put in render list)
#define SOF_ForceVisible        0x00010000      // CAT: was 0x40000. object will always be in render list no matter what
#define SOF_BigObject           0x00020000      // CAT: was 0x80000. Indicates object is big (e.g. A Mothership)

#define SOF_StaticInfoIsDynamic 0x00040000      // CAT: was 0x100000. Indicates static info has been copied and modified and is dynamic

#define SOF_NotBumpable         0x00080000      // CAT: was 0x200000. Indicates colliding with object should not result in momentum transferal

#define SOF_Targetable          0x00100000      // CAT: was 0x400000. Indicates object is targetable

// CAT: SOF_Slaveable (HW1 0x800000) has no bit: Cataclysm has no slave trade (dockMakeMaster, dockAddSlave,
//      dockRemoveSlave ... are absent from the exe). 0 leaves the HW1 slave code inert until stage 2 removes it.
#define SOF_Slaveable           0x00000000      // CAT: retired

#define SOF_Crazy               0x00200000      // CAT: was 0x1000000. Indicates object is in crazy mode, and about to die. (Used for ships only)

#define SOF_DontDrawTrails      0x00400000      // CAT: was 0x2000000. Indicates we do not want to draw engine trails. (Used for ships only)

#define SOF_Disabled            0x00800000      // CAT: was 0x4000000. set if ship is to be a derelict in space

#define SOF_Clamped             0x01000000      // CAT: was 0x8000000. indicates ship is CLAMPED to another ship

#define SOF_Hyperspace          0x02000000      // CAT: was 0x10000000. indicates ship is in hyperspace ether

#define SOF_AlwaysSortFront     0x04000000      // CAT: was 0x20000000. object will always sort to the front

#define SOF_HideSave            0x08000000      // CAT: was 0x40000000. object was hidden
#define SOF_HideSave2           0x10000000      // CAT: was 0x80000000. object was hidden

// CAT: three new flags (names chosen here; the exe's are unknown):
//      0x20000000: an effect that follows its owner's coordinate system. etgEffectCreate 0x00470990 maps the
//      EAF_OwnerCoordSys (0x01000000) argument bit to it and etgEffectDraw tests it; HW1 stored the EAF bit itself in
//      the header flags, where it collided with SOF_Crazy.
#define SOF_OwnerCoordSys       0x20000000
//      0x40000000: a ship that is a part of another ship (cat_univCreateMothershipModules, cat_dreadnoughtCreateCannon
//      set it; DreadnoughtDied, cat_shipDissolveStart clear it; ApplyDamageToTarget 0x00546250 redirects damage).
#define SOF_CatShipPart         0x40000000
//      0x80000000: docking/attached (cat_dockShipDocksAtShip, cat_processLinkToDo, cat_leechProcess,
//      cat_moduleShipEject set it; RemoveShipFromDocking/Launching, LaunchCleanup, processLaunchShipToDo clear it;
//      collCheckShipShipColl, collCheckBigShipSmallShipColl, collCheckShipResourceColl skip such ships).
#define SOF_CatDocking          0x80000000

typedef struct
{
    Node objlink;
    ObjType objtype;        // object type (ship or bullet, etc)
    udword flags;           // flags whether the object is rotatable, etc.
    // CAT: two new header dwords before staticinfo (the exe's objects have staticinfo at +0x20, HW1 +0x18):
    //      +0x18 is cleared by etgEffectCreate 0x004709ed (meaning not known yet); +0x1c is set to 1 by the ETG
    //      callable omitParentShipFromRenderList (cat_etgParentShipOmitFromRenderList 0x00470190) and cleared for
    //      world-rendered effects (0x00470ba3); its reader is the render list builder (not ported yet)
    udword catHeader18;
    udword catOmitFromRenderList;
} SpaceObjHeader;

/*-----------------------------------------------------------------------------
    CAT: per-vertex runtime mesh state (the "glow" state). cat_univShipMeshStateCreate 0x00541cd0 gives every ship,
    and only ships, a table with one entry per LOD level; each mesh level holds one 12-byte slot per polygon object
    (numbered in the hierarchy's pre-order, as the render walk numbers them) with a 0x28-byte state per vertex
    (cat_univMeshStateAllocRecursive 0x00541b80). The renderer passes the vertex state to cat_shColourAddGlow
    0x0050d7c0; cat_shGlowUpdate 0x0050d560 and the bullet-hit callback 0x004b5820 change it. Field names are chosen
    here from those users.
-----------------------------------------------------------------------------*/
typedef struct CatVertexState
{
    real32 scorch;          // +0x00 darkening: grows by 0.2 x the hit glow lost when the vertex info has flag 0x02,
                            //       decays 0.1 per second, clamped to 0..1 (cat_shGlowUpdate)
    real32 hitGlow;         // +0x04 the bullet-hit callback adds 1.0; decays 0.2 per second, at most 1.0
    real32 value08;         // +0x08 1.0 when the vertex info has flag 0x04 (no shipped mesh has it), else 0; no reader found
    real32 value0C;         // +0x0C 0, no reader found
    ubyte value10;          // +0x10 0, no reader found
    ubyte dirty;            // +0x11 1 at creation and when the info has flags 0x31: cat_shGlowUpdate recomputes the colour
    ubyte pad12[2];
    real32 lastUpdate;      // +0x14 universe.totaltimeelapsed at the last cat_shGlowUpdate, -1.0 at creation
    real32 colour[3];       // +0x18 R, G, B offsets cat_shColourAddGlow adds (x 256 x 2) to the lit colour
    real32 value24;         // +0x24 0, no reader found
} CatVertexState;

typedef struct CatMeshObjectState
{
    sdword objectWord0;     // +0x00 a copy of the polygon object's first word (its name pointer in the exe)
    sdword nVertices;       // +0x04
    CatVertexState *vertex; // +0x08 nVertices states
} CatMeshObjectState;

typedef struct CatMeshState
{
    sdword nLevels;                     // +0x00 the ship's LOD count
    CatMeshObjectState *level[1];       // +0x04 per LOD level: the object slots of a mesh level, NULL otherwise
} CatMeshState;

#define catMeshStateSize(n)     (sizeof(CatMeshState) + sizeof(CatMeshObjectState *) * ((n) - 1))

_Static_assert(sizeof(CatVertexState) == 0x28, "the per-vertex mesh state is 0x28 bytes in cataclysm.exe");
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatMeshObjectState) == 0x0C, "the per-object mesh state slot is 12 bytes in cataclysm.exe");
#endif

typedef struct
{
    SpaceObjHeader header;
    StaticInfo *staticinfo; // pointer to static info
    Node renderlink;        // link for rendering list
    ubyte currentLOD;       // current level of detail
    ubyte renderedLODs;     //what LODs have been rendered already
    uword attributes;               // settable attributes from mission editor
    sword attributesParam;           // parameter for attributes, set from mission editor
    ubyte attributesPad[2];          // leftover, free space
    real32 cameraDistanceSquared;    // distance to camera (for sound, level of detail)
    vector cameraDistanceVector;     // vector to camera (camera eyeposition - obj->posinfo.position)
    real32 collOptimizeDist;         // distance used for optimizing collision checking
    struct blob *collMyBlob;         // collision blob I belong to
    PosInfo posinfo;
    // CAT: two new fields right after posinfo (exe +0x80 and +0x84; posinfo is +0x58..+0x80 in the exe). Every
    //      creator sets both (univAddAsteroid 0x00540a30, univAddMissile 0x00540c70, etgEffectCreate 0x00470990 ...)
    //      and LoadSpaceObj 0x00501660 clears both after a load.
    CatMeshState *catMeshState;      // exe +0x80: ships only (cat_univShipMeshStateCreate 0x00541cd0), else NULL;
                                     //   freed by cat_univMeshStateFree 0x00541df0
    ubyte *catEffectBlock;           // exe +0x84: a one-byte block the effect creators allocate (etgEffectCreate,
                                     //   etgCreateEffects, etgSpawnNewEffect: memAlloc(1)), NULL for every other
                                     //   object; every free path frees it. No reader was found (render, stage 3)
} SpaceObj;

typedef SpaceObj *SpaceObjPtr;

/*=============================================================================
    SpaceObj - rotatable
=============================================================================*/

typedef struct
{
    // https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1548.pdf § 6.5.2.3 Structure and union members
    // "If the member used to read the contents of a union object is not the same as the member last used to
    // store a value in the object, the appropriate part of the object representation of the value is reinterpreted
    // as an object representation in the new type"
    // So, since C11, accessing a member of a union is guaranteed to reinterpret the memory at that location.
    // As long as we guarantee the fields to be the same, then we should be safe.
    union {
        SpaceObjHeader header;
        SpaceObj obj;
    };
    RotInfo rotinfo;
} SpaceObjRot;

/*=============================================================================
    SpaceObj - rotatable, impactable
=============================================================================*/

typedef struct
{
    union {
        SpaceObjHeader header;
        SpaceObj obj;
        SpaceObjRot rotatable;
    };
    CollInfo collInfo;
    Node impactablelink;
} SpaceObjRotImp;

/*=============================================================================
    SpaceObj - rotatable, impactable, targetable
=============================================================================*/

typedef struct SpaceObjRotImpTarg
{
    union {
        struct {
            SpaceObjHeader header;
            // Reinterprets `SpaceObj.staticinfo`
            StaticInfoHealth* staticinfo;
        };
        SpaceObj obj;
        SpaceObjRot rotatable;
        SpaceObjRotImp impactable;
    };
    real32 health;                  // hitpoints
    real32 flashtimer;              // determines if object flashes on screen
    real32 lasttimecollided;
} SpaceObjRotImpTarg;

typedef SpaceObjRotImpTarg *TargetPtr;

/*=============================================================================
    SpaceObj - rotatable, impactable, targetable, guidance
=============================================================================*/

typedef struct SpaceObjRotImpTargGuidance {
    union {
        struct {
            SpaceObjHeader header;
            // Reinterprets `SpaceObj.staticinfo`
            StaticInfoHealthGuidance* staticinfo;
        };
        SpaceObj obj;
        SpaceObjRot rotatable;
        SpaceObjRotImp _impactable;
    };
    // Duplicates fields of SpaceObjRotImpTarg to keep everything byte-aligned with the previous implementation
    // and maintain savegame compatibility
    struct {
        real32 _health;
        real32 _flashtimer;
        real32 _lasttimecollided;
    };
    NonStats nonstatvars;
    ClampInfo* clampInfo;
} SpaceObjRotImpTargGuidance; // used by missiles, ships, derelicts

typedef struct ShipOrDerelict
{
    union {
        struct {
            SpaceObjHeader header;
            // Reinterprets `SpaceObj.staticinfo`
            StaticInfoShipOrDerelict* staticinfo;
        };
        SpaceObj obj;
        SpaceObjRot rotatable;
        SpaceObjRotImp impactable;
        SpaceObjRotImpTarg targetable;
        SpaceObjRotImpTargGuidance guidance;
    };

    SalvageInfo *salvageInfo;       //salvage info...

    //must come next to be compatible with Derelict in aiship stuff
    ubyte aistate;                  // AI state variable
    ubyte pad1[2];
    sbyte putOnDoor;
    struct Ship *dockingship;       // if non-NULL, indicates no collisions should occur with this ship
    udword specialFlags;            //general purpose ship flags, see above for defines
    udword specialFlags2;           //more general purpose ship flags, see above for defines
    sdword salvageNumTagged[MAX_MULTIPLAYER_PLAYERS];        //number of salcaps vying for ship...used to disperese them amonst many targets
} ShipOrDerelict;   // ships, derelicts

typedef struct TargetID
{
    uword objNumber;    // must be first member of structure to be compatible with other ID's
    uword objtype;
} TargetID;

/*=============================================================================
    Repair Droid Stuff:
=============================================================================*/
/*
typedef struct RepairDroid
{
    udword flags;       // RD Flags...
    PosInfo posinfo;    // Position
    RotInfo rotinfo;    // Rotation
} RepairDroid;
*/
/*-----------------------------------------------------------------------------
    Ship Object
-----------------------------------------------------------------------------*/

#define ATTACKEVENT_ENEMYTARGET_DIED    1
#define ATTACKEVENT_LEADER_DIED         2
#define ATTACKEVENT_WINGMAN_DIED        4

#define ATTACKSITUATION_EVADINGREPOS    1
#define ATTACKSITUATION_JUSTEVADING     2
#define ATTACKSITUATION_SPLITTING       3
#define ATTACKSITUATION_SANDWICH        4

//  universe ticks before a ship "forgets" that it was under attack
//  (used by the recentlyAttacked member -- keep this within a ubyte for now)
#define RECENT_ATTACK_DURATION        ((ubyte)(5 * UNIVERSE_UPDATE_RATE))

typedef struct
{
    sdword numAttackTargets;
    TargetPtr TargetPtr[1];
} AttackTargets;

#define sizeofAttackTargets(n) (sizeof(AttackTargets) + (n-1)*sizeof(TargetPtr))

typedef struct
{
    AttackTargets *multipleAttackTargets;   // only used by ships which can target multiple targets.
    SpaceObjRotImpTarg *attacktarget;
    struct Ship *myLeaderIs;
    struct Ship *myWingmanIs;
    uword flightmansLeft;       // number of flight maneuvers left
    uword attacksituation;      // what I am currently doing
    uword attackevents;         // event notification using ATTACKEVENT flags
    uword pad;
} SpecificAttackVars;

typedef struct
{
    struct Ship *dockship;
    struct Ship *busyingThisShip;
    vector destination;
    ubyte dockstate;
    ubyte dockstate2;
    ubyte dockstate3;
    sbyte reserveddocking;
    DockStaticPoint *dockstaticpoint;
    void *customdockinfo;
    sdword **launchpoints;
} SpecificDockVars;

typedef struct ShipID
{
    uword shipNumber;
} ShipID;

#define MAX_ID_NUMBER   65535

#define MAD_MAX_CUEABLE 2           //maximum number of animations that can be cued in a row

//Ship death flags for flag 'howDidIDie'
//howDidIDie == 0 ==> not dead, or didn't die by these means!
#define DEATH_Killed_By_Player          666 //another player killed this person (could be self..check other variable)
#define DEATH_Killed_By_Player_Explosion 667
#define DEATH_Killed_By_Kamikaze        668
#define DEATH_I_DIDNT_DIE_I_WAS_SALAVAGED 1004 //ship was salvaged!  So deal with gamestats accordingly
//non player dependent
#define DEATH_Killed_By_Collision       777 //ship raninto another ship/asteroid/object and died
#define DEATH_Killed_By_Cloud           888 //Dust Cloud damage kills a ship
#define DEATH_Killed_By_Dead_Player     999 //player that did killing is dead and unreferenceable
#define DEATH_BY_GAME_END               1000 //game ended, ship was freed
#define DEATH_BY_RETIREMENT             1001 //ship was retired by player
#define DEATH_BY_AFTER_DEAD             1002 //ship self destructed after player was already dead
#define DEATH_Left_Behind_At_Hyperspace 1003 //ship didn't finish docking in time for hyperspace

//If howDidIDie isn't set with the above, it is assumed to have been POOFED out of existence.
//#define DEATH_By_Poofing                888 //somewheres in the code this ship was poofed out of existence.

//ship struct "specialflags" defines
#define SPECIAL_StopForResearchDocking              BIT0
#define SPECIAL_Kamikaze                            BIT1
#define SPECIAL_BrokenFormation                     BIT2
#define SPECIAL_KamikazeCrazyFast                   BIT3
#define SPECIAL_SpeedBurst                          BIT4
#define SPECIAL_SpeedBurstCooling                   BIT5
#define SPECIAL_BurstFiring                         BIT6
#define SPECIAL_SinglePlayerWithinHyperSpaceRange   BIT7
#define SPECIAL_SinglePlayerLimitSpeed              BIT8
#define SPECIAL_BUSY_BECAUSE_SALVAGE_DOCKING        BIT9
#define SPECIAL_Harvested                           BIT10
#define SPECIAL_EXITING_DOOR                        BIT11
#define SPECIAL_STAY_TILL_EXPLICITLAUNCH            BIT12
#define SPECIAL_KasSpecialDamageModifier            BIT13
#define SPECIAL_Finished_Resource                   BIT14  //flag indicates when a resource collector has just finished collecting resources from one particular resource (computer player specific)
#define SPECIAL_Resourcing                          BIT15  //flag indicates when a resource collector has started resourcing - computer player specific
#define SPECIAL_SinglePlayerInParade                BIT16
#define SPECIAL_rowGettingOutOfWay                  BIT17
#define SPECIAL_ATTACKMOVECANCEL                    BIT18
#define SPECIAL_FriendlyStatus                      BIT19  //set by KAS - player's ships don't autoretaliate against this ship.  Also allows you to dock with them.
#define SPECIAL_IsASalvager                         BIT20  //used by junkyard dawg and salcap corvette...elminates double checks of the ship types!
#define SPECIAL_SalvagerHasSomethingAttachedAndIsDoingSomethingElse BIT21
#define SPECIAL_AttackFromAbove                     BIT22
#define SPECIAL_SalvagedTargetGoingIntoDockWithShip BIT23
#define SPECIAL_SalvageTakingHomeTechnology         BIT24
#define SPECIAL_Hyperspacing                        BIT25
#define SPECIAL_ForcedAttackStatus                  BIT26  //ship is in combat status, despite not actually attacking ships (must be turned off - doesn't expire)

// CAT (M2-R3 ships-a): the exe's specialFlags bit 19 (0x80000), set on every Cataclysm salvager by cat_salvageInit
//      0x004f9010. It takes BIT27 (free in this tree) and not HW1's SPECIAL_IsASalvager (BIT20), because HW1 code that
//      tests SPECIAL_IsASalvager reads HW1's SalCapCorvetteSpec from the ship; until the M2-R4 #2 renumbering
#define SPECIAL_CatSalvager                         BIT27
#define SPECIAL_LaunchedFromKas                     BIT28
#define SPECIAL_KasCheckDoneLaunching               BIT29
#define SPECIAL_Launching                           BIT30  //indicates ship is launching
#define SPECIAL_ParadeNeedTomoveCloser              BIT31


//////////SPECIAL_2 Flags
#define SPECIAL_2_SalvageTargetAlreadyDriven        BIT0
#define SPECIAL_2_ShipInGravwell                    BIT1
#define SPECIAL_2_InUnitCaps                        BIT2
#define SPECIAL_2_CircularGuard                     BIT3
#define SPECIAL_2_NoMoreDockingAllowed              BIT4
#define SPECIAL_2_DontPlowThroughEnemyShips         BIT5
#define SPECIAL_2_BusiedDockPoint                   BIT6
#define SPECIAL_2_DisabledForever                   BIT7
// CAT (M2-R3 selection): two Cataclysm specialFlags2 states the selection filters test. The exe keeps them at bit 3
//      (0x8: ship being built externally, cleared by cat_clExternalBuildUpdate 0x0044dfc0; HW1's CircularGuard bit, which
//      Cataclysm moved into specialFlags, clProtect 0x0044af20) and bit 13 (0x2000: ship being salvaged,
//      cat_salvageIsBeingSalvaged 0x004f7c00). The specialFlags/specialFlags2 renumbering belongs to the CommandLayer
//      port (M2-R4 #2); until then they take free bits of this tree's specialFlags2.
#define SPECIAL_2_CatBeingBuilt                     BIT8
#define SPECIAL_2_CatBeingSalvaged                  BIT9
// CAT (M2-R3 ships-a): the exe's specialFlags2 bit 13 is also set by cat_salvageDepotInit 0x004fc7c0 on every salvage
//      depot (carriers, motherships, processors, bHeavyCruiser), so it marks a ship as a depot; this name says so. Same
//      bit as SPECIAL_2_CatBeingSalvaged (cat_salvageIsBeingSalvaged 0x004f7c00 only tests it).
#define SPECIAL_2_CatSalvageDepot                   SPECIAL_2_CatBeingSalvaged
// CAT (M2-R3 ships-a): the exe's specialFlags2 bit 15, set by kasfShipsSetBeastInfectionUnlimited 0x00498a60 and
//      cleared by cat_beastWeaponInit 0x0042a040; a free bit of this tree until the M2-R4 #2 renumbering
#define SPECIAL_2_CatBeastInfectionUnlimited        BIT10
// CAT (M2-R3 ships-a): the exe's specialFlags2 bit 20 (0x100000). Ships with it get no mothership health warning
//      (MothershipHouseKeep 0x005bcad0), no battle pings or death screams (battlePingEvaluate 0x00424110), no support-
//      unit accounting (cat_suShipAdded 0x00459a90) and special docking rules (ThisShipCanDockWith 0x00464290). Its
//      setter and meaning are not pinned; a free bit of this tree until the M2-R4 #2 renumbering
#define SPECIAL_2_CatExempt20                       BIT11

// CAT (M2-R3 selection): Ship.catShipFlags (exe Ship+0x18)
#define CAT_SHIPFLAG_Mimic                          BIT3    // set by MimicInit 0x005b8360, MCVInit 0x005b8130, P1StandardCorvetteInit 0x005be660
#define CAT_SHIPFLAG_KasDerelict                    BIT4    // kasfShipsSetDerelict 0x00498510
#define CAT_SHIPFLAG_NoTactics                      BIT1    // cat_shipCanSetTactics 0x005135e0 refuses it (setter not located)
// CAT (M2-R3 ships-a): set on the component-class ships clamped to a carrier or mothership when the host dies
//      (CarrierDied 0x005af3f0, MothershipHouseKeep 0x005bcad0); read by cat_salvageCheckTarget 0x004f7eb0 and
//      cat_kamikazeSteer 0x005b16f0. Name chosen here.
#define CAT_SHIPFLAG_Orphaned                       BIT2

// CAT (M2-R3 selection): number of Ship.catDisableReason slots (cat_shipSetDisableReason 0x005b3fe0 scans 13 dwords)
#define CAT_NUM_DISABLE_REASONS                     13

// CAT (M2-R3 selection): Ship.catUpgradeFlags bits the selection code tests (names chosen here from their users)
#define CAT_UPGRADE_Leech                           0x00000040  // cat_shipCanLeech 0x005136a0
#define CAT_UPGRADE_Ability1                        0x00001000  // cat_selMakeShipsHaveAbilityUpgrade 0x00512330
#define CAT_UPGRADE_Ability2                        0x00002000
#define CAT_UPGRADE_Ability3                        0x00004000
#define CAT_UPGRADE_Repair                          0x00040000  // cat_shipCanRepair 0x005138b0
#define CAT_UPGRADE_Salvage                         0x00080000  // cat_shipCanSalvage 0x005138f0
#define CAT_UPGRADE_AcolyteLinkFire                 0x04000000  // cat_acolyteCanFireLinkedWeapon 0x005ada50
				

struct Resource;

typedef struct Ship         // Ship object
{
    union {
        struct {
            SpaceObjHeader header;
            // Reinterprets `SpaceObj.staticinfo`
            ShipStaticInfo* staticinfo;
        };
        SpaceObj obj;
        SpaceObjRot rotatable;
        SpaceObjRotImp impactable;
        SpaceObjRotImpTarg targetable;
        SpaceObjRotImpTargGuidance guidance;
        ShipOrDerelict shipOrDerelict;
    };
    //TODO(type safety, 2026-04-24): Merge into ShipOrDerelict
    NAVLightInfo *navLightInfo;
    // HERE UP SAME for Derelicts and Ships

    Node shiplink;
    vector moveTo;                  // move destination of the ship
    vector moveFrom;                // where the ship is moving from
    vector formationOffset;         // offset in formation
    vector formationHeading;        // heading in formation (optional)
    vector engineOffset;    // engine nozzle offset from position in object coordinates.
    vector enginePosition;  // engine position in universe
    struct Ship *rowGetOutOfWay;    // Right of way - ship to get out of way of
    vector rowOriginalPoint;        // Right of way - my original point
    vector rowOutOfWayPoint;        // Right of way - point to go to be out of way
    ShipType shiptype;
    ShipRace shiprace;
    //*******Tactics stuff start
    //FIX LATER to support fewer variables using bitmasks
    TacticsType tacticstype;        //ships current tactcis setting
    bool32 isDodging;                 //flag, true if ship is performing a dodge
    real32 DodgeTime;               //variable used to control when a ship will stop dodging
    uword  DodgeDir;
    uword  pad2;
    bool32 tacticsFormationVar1;      //variable for formation attacking
    udword DodgeFlag;               //info flag for dodging
    sdword tactics_ordertype;       //ships order type convulved into the Tactics Domain
    udword tacticsTalk;             //var used for intership comunication
    sdword tacticsNeedToFlightMan;  //same as above
    //*******Tactics stuff end
    sdword dockwaitforNorm;
    real32 timeCreated;
    sdword kamikazeState;
    vector kamikazeVector;
    real32 speedBurstTime;
    real32 singlePlayerSpeedLimiter;  // limiter of ships maxvelocity, single player game only!
    real32 deathtime;       // time at which this ship should blow up, if SOF_Crazy is set
    //mad animation variabes
    udword madAnimationFlags;       //general purpose flags for mad animations
    udword madGunStatus;            //gun status
    udword madWingStatus;           //wing animation status
    udword madDoorStatus;
    udword madSpecialStatus;
    udword cuedAnimationIndex;            //Animation index waiting in the wings to be played
    udword cuedAnimationType;      //cue of animation types (guns, wings, etc...)
    udword nextAnim;
    udword soundEventAnimationTypeFlag;    //opening,closing,damaged_opening,damaged_closing
    sdword forceCancelDock;
    real32 gravwellTimeEffect;      //time the gravwell effect sould be played again
    //
    struct Player *playerowner;
    sdword resources;
    real32 fuel;
    real32 lastTimeRepaired;
    real32 magnitudeSquared;        // most recent scaling factor
    struct Ship *gettingrocked;     // ship which is rocking you (shooting you)
    struct Ship *recentAttacker;    // ship which has attacked you recently (ie, not just this AI tick) -- see recentlyAttacked
    struct Ship *firingAtUs;        // ship which has recently fired upon us (but needn't have hit) see recentlyFiredUpon
    bool8 shipidle;                 // flag to indicate if ship has no command to execute
    bool8 autostabilizeship;        // if TRUE, we should stabilize the ship
    ubyte autostabilizestate;       // state of autostabilization
    ubyte autostabilizewait;        // how many frames to wait before stabilizing in autostabilize
    ubyte aistateattack;            // AI attack state variable
    ubyte flightmanState1;          // generic state variables for flight maneuvers
    ubyte flightmanState2;          // generic state variables for flight maneuvers
    ubyte rcstate1;                 // state variable for resource collecting
    ubyte rcstate2;                 // state variable for resource collecting
    ubyte aistatecommand;
    sbyte dontrotateever;
    sbyte dontapplyforceever;
    bool8 shipisattacking;
    sbyte colorScheme;              //what color scheme does this ship use?
    ubyte recentlyAttacked;         // counts down from RECENT_ATTACK_DURATION -- see recentAttacker
    ubyte recentlyFiredUpon;        // same, see firingAtUs
    ubyte chatterBits;              // flags for battle chatter
    ubyte rowState;                 // state variable for right of way
    real32 aidescend;               // AI variable for capital ships descending
    udword flightman;               // current flight maneuver handle
    void *flightmanInfo;            // pointer to any additional flight maneuver info
    struct CommandToDo *formationcommand;   // pointer to formationcommand, if ship in formation
    struct CommandToDo *command;    // pointer to ship's current command, if any
    SpecificAttackVars attackvars;  // attack information
    SpecificDockVars dockvars;      // dock information
    struct Effect *rceffect;        // effect variable for resource collecting
    void (*ShipXHarvestsResourceY)(struct Ship *ship,struct Resource *resource);    // function pointer for resource collecting
    sdword visibleToWho;              //bitmask flag indicating to which players this ship is visible to...relevant only if ship is cloaked
    sdword visibleToWhoPreviousFrame; //same as above, but relevant for the previous univupdate frame
    sdword howDidIDie;              //defined above: Set when a ship is set to die, that way, at somepoint (hopefully) we can tell how the bitch died.
    sdword whoKilledMe;             //playerIndex of the player that killed this ship
    udword nDamageLights;
    DamageLightStatic* damageLights;
    struct Effect* showingDamage[3][3];
    vector holdingPatternPos;
    real32 damageModifier;
    real32 passiveAttackCancelTimer;

    //later will make most of these into a single flag
    udword tractorbeaminfo;
    udword tractorvector;
    struct Player *tractorbeam_playerowner;
    sdword tractorstate;            //info variable for tractorbeaming state
    sdword tractorPosinfo[4];        //more tractor info!
    sdword hyperspacePing;          //true or false flag indicating if the ship has had a hyperspace ping created for it...NON-deterministic!

    sdword needNewLeader;
    struct Ship *newDockWithTransfer;
    sdword newDockIndexTransfer;

    real32 shipDeCloakTime;         //time is set when a ship decloaks...this time is used to give decloaking ships an advantage
    shiptrail* trail[MAX_NUM_TRAILS];   //ship's engine trail(s)
    ShipID shipID;
    uword hotKeyGroup;              // what hot-key group this ship belongs to
    sdword retaliateSpeechVar;
    real32 retaliateSpeechTime;

    SOUNDEVENT soundevent;
    Node slavelink;                 //Nodal link for slavery info
    SlaveInfo *slaveinfo;           //pointer to struct of info regarding ships slavery status
    GunInfo *gunInfo;
    DockInfo *dockInfo;
    ShipsInsideMe *shipsInsideMe;
    shipbindings *bindings;         //ship hierarchy bindings
    madanim *madBindings;
//    RepairDroid *pRepairDroids;     // Alex's repair droid stuff...
    struct ShipSinglePlayerGameInfo *shipSinglePlayerGameInfo;     // stuff for the single player game
    void* lightning[2];             //lightning effect stuff
    // CAT (M2-R3 selection): per-ship Cataclysm state read by the selection filters (ShipSelect.c, Select.c). The exe
    //      keeps these in its own Ship layout (0x63C bytes, univCreateShip 0x00542320 zeroes them); this tree's Ship
    //      is GoK's, so they are appended here by name. Their setters are ported with the code that owns them.
    udword catShipFlags;            // exe Ship+0x18, CAT_SHIPFLAG_* below
    sdword catDisableReason[CAT_NUM_DISABLE_REASONS];   // exe Ship+0x1DC, cat_shipSetDisableReason 0x005b3fe0 sets [reason]
    bool32 catDisabled;             // exe Ship+0x210: TRUE while any catDisableReason[] is nonzero (cat_shipSetDisableReason
                                    //   0x005b3fe0, cat_shipSetHelpless 0x005b3bd0, cat_shipClearHelpless 0x005b3c80,
                                    //   cat_empShockShip 0x005b3900, cat_shipUpdateDrift 0x005b3d60 recompute it)
    udword catUpgradeFlags;         // exe Ship+0x3F4: CAT_UPGRADE_* (cat_shipHasUpgrade 0x00514210; cat_shipUpgradesInit 0x005140a0 clears it)
    bool32 catDissolving;           // exe Ship+0x570: set by cat_shipDissolveStart 0x004603d0, cleared by cat_shipDissolveInit 0x00460220
    bool32 catUpgrading;            // exe Ship+0x598: set by cat_clUpgradeShips 0x0044de80, cleared by cat_univShipDeferredUpdate 0x005447b0
    udword sizeofShipSpecifics;
    udword ShipSpecifics[1];        //this member must go last
} Ship;

typedef Ship *ShipPtr;

/*-----------------------------------------------------------------------------
    Missile Object
-----------------------------------------------------------------------------*/

typedef struct MissileID
{
    uword missileNumber;
} MissileID;

typedef struct
{
    union {
        struct {
            SpaceObjHeader header;
            // Reinterprets `SpaceObj.staticinfo`
            MissileStaticInfo* staticinfo;
        };
        SpaceObj obj;
        SpaceObjRot rotatable;
        SpaceObjRotImp impactable;
        SpaceObjRotImpTarg targetable;
        SpaceObjRotImpTargGuidance guidance;
    };

    //missile trail stuff
    missiletrail* trail;
    vector enginePosition;

    Node missilelink;
    MissileType missileType;
    GunSoundType soundType;
    ShipRace race;
    real32 maxtraveldist;
    real32 timelived;
    real32 totallifetime;
    struct Player *playerowner;
    Ship *owner;
    SpaceObjRotImpTarg *target;
    etglod *hitEffect;                          //effect to play on impact
    real32 damage;
    sdword haveCheckedForLoss;
    MissileID missileID;
    uword pad2;
    udword mineAIState;                         //ai state of missile MINES
    vector formation_position;                  //vector that records the position the mine WANTS to be in while in formation
    sdword FORCE_DROPPED;                       //flag..true if force_dropped...used for variable selection
    Node formationLink;
    MineFormationInfo *formationinfo;
    sdword formation_number_X;
    sdword formation_number_Y;
    ubyte colorScheme;
    ubyte pad[3];
} Missile;

typedef Missile *MissilePtr;

/*-----------------------------------------------------------------------------
    Effect Object
-----------------------------------------------------------------------------*/

typedef struct Effect
{
    union {
        struct {
            SpaceObjHeader header;
            // Reinterprets `SpaceObj.staticinfo`
            etgeffectstatic* staticinfo;
        };
        SpaceObj obj;
        SpaceObjRot rotatable;
    };

    //... effect-specific stuff
    Node effectLink;
    udword effectFlags;                         //effects-specific flags
    //TODO(type safety, 2026-04-24): Fix this pointer's type. There are some unsafe casts around (e.g. from Bullet*)
    Ship *owner;                                //owner of this effect (e.g. ship that blew up)
    real32 magnitudeSquared;                    //N-lips scale factor, if any
    sdword nSpawnedAttachedToOwner;             //number of spawned effects attached to this effect's owner
    real32 timeElapsed;                         //age of effect (can be negative for tim-offset effects)
    vector LoF;                                 //line-of-fire vector
    vector velocity;                            //velocity vector
    real32 theta;                               //R-plane rotation vector

    real32 drag;                                //drag for moving entire effect

    real32 spin;                                //control the spin of the effect
    real32 deltaSpin;

    ubyte *variable;                            //variable block
    ubyte *variableRate;                        //variable rate block (effectors)
    ubyte *effectID;                            //effector ID block for variables
    sdword nParticleBlocks;                     //max number of particle systems
    sdword iParticleBlock;                      //current number of particle systems
    ubyte *particleBlock[1];                    //ragged array of particle systems in effect
} Effect;

/*-----------------------------------------------------------------------------
    Bullet Object
-----------------------------------------------------------------------------*/

typedef struct                                  // Bullet object
{
    union {
        SpaceObjHeader header;
        SpaceObj obj;
        SpaceObjRot rotatable;
    };

    Node bulletlink;
    vector bulletheading;                       // heading in universe
    vector lengthvec;
    real32 lengthmag;
    real32 traveldist;
    real32 beamtraveldist;                      // only used for BULLET_Beam
    BulletType bulletType;
    GunSoundType soundType;
    real32 timelived;
    real32 totallifetime;
    Ship* owner;                                // pointer to bullet's owner
    Gun *gunowner;                              // pointer to bullet's gun from which it was fired
    SpaceObjRotImpTarg *target;                 // target of bullet
    color bulletColor;                          // race of player who shot bullet
    real32 bulletmass;
    etglod *hitEffect;                          //effect to play when it hits
    Effect *effect;                             //effect to play as it flies
    real32 damage;                              //current damage of bullet
    real32 damageFull;                          //unadjusted (initial) damage
    udword SpecialEffectFlag;                   //Used by special ships to see if they have affected this bullet
    real32 DFGFieldEntryTime;                   //used by DFGF.
    real32 BulletSpeed;
    struct Player *playerowner;
} Bullet;

typedef Bullet *BulletPtr;

/*=============================================================================
    Derelict
=============================================================================*/

typedef struct DerelictID
{
    uword derelictNumber;
} DerelictID;

typedef struct
{
    union {
        struct {
            SpaceObjHeader header;
            // Reinterprets `SpaceObj.staticinfo`
            DerelictStaticInfo* staticinfo;
        };
        SpaceObj obj;
        SpaceObjRot rotatable;
        SpaceObjRotImp impactable;
        SpaceObjRotImpTarg targetable;
        SpaceObjRotImpTargGuidance guidance;
        ShipOrDerelict shipOrDerelict;
    };
    NAVLightInfo *navLightInfo;
    // HERE UP SAME for Derelicts and Ships

    Node derelictlink;
    DerelictID derelictID;
    sbyte colorScheme;              //what color scheme does this ship use?
    ubyte pad[3];

    sdword ambientSoundHandle;      //used by sound engine to keep track of the ambient sound the derelict makes
    sdword randomSoundHandle;       //used by sound engine to keep track of the random ambient sound the derelict makes
    real32 nextRandomTime;          //time to play the next random ambient sound

    sdword TractorTrue;             //used to stop friction on derelicts when being pushed
    udword tractorbeaminfo;         //counter for # of tractor beams on derelict
    udword tractorvector;
    struct Player *tractorbeam_playerowner;
    sdword tractorstate;            //info variable for tractorbeaming state
    sdword tractorPosinfo[4];        //more tractor info!
    sdword needNewLeader;
    struct Ship *newDockWithTransfer;
    sdword newDockIndexTransfer;
    real32 creationTime;
    DerelictType derelicttype;
} Derelict;

typedef Derelict *DerelictPtr;

/*=============================================================================
    Resource Objects
=============================================================================*/

typedef struct ResourceID
{
    uword resourceNumber;
} ResourceID;

/*-----------------------------------------------------------------------------
    Generic Resource, from which all resources (asteroids, nebula, etc. are derived)
-----------------------------------------------------------------------------*/

typedef struct Resource
{
    union {
        struct {
            SpaceObjHeader header;
            // Reinterprets `SpaceObj.staticinfo`
            ResourceStaticInfo* staticinfo;
        };
        SpaceObj obj;
        SpaceObjRotImp impactable;
        SpaceObjRotImpTarg targetable;
    };

    Node resourcelink;
    ResourceID resourceID;
    uword resourceNotAccessible;
    sdword resourcevalue;
    struct ResourceVolume *resourceVolume;      // resourceVolume I belong to
} Resource;

typedef Resource *ResourcePtr;    // all resources are assumed to be rotatable, and impactable

/*-----------------------------------------------------------------------------
    Asteroid
-----------------------------------------------------------------------------*/

typedef struct Asteroid
{
    union {
        struct {
            SpaceObjHeader header;
            // Reinterprets `SpaceObj.staticinfo`
            AsteroidStaticInfo* staticinfo;
        };
        SpaceObj obj;
        SpaceObjRot rotatable;
        SpaceObjRotImp impactable;
        SpaceObjRotImpTarg targetable;
        Resource resource;
    };

    AsteroidType asteroidtype;  // asteroid specific information
    real32 scaling;
} Asteroid;

/*-----------------------------------------------------------------------------
    Nebula
-----------------------------------------------------------------------------*/

typedef struct Nebula
{
    union {
        struct {
            SpaceObjHeader header;
            // Reinterprets `SpaceObj.staticinfo`
            NebulaStaticInfo* staticinfo;
        };
        SpaceObj obj;
        SpaceObjRot rotatable;
        SpaceObjRotImp impactable;
        SpaceObjRotImpTarg targetable;
        Resource resource;
    };

    // put nebula specific stuff here only
    NebulaType nebulatype;
    real32 scaling;
    nebChunk* stub;
    sdword nebNebulaeIndex;
} Nebula;

/*-----------------------------------------------------------------------------
    Gas Cloud
-----------------------------------------------------------------------------*/

typedef struct GasCloud
{
    union {
        struct {
            SpaceObjHeader header;
            // Reinterprets `SpaceObj.staticinfo`
            GasCloudStaticInfo* staticinfo;
        };
        SpaceObj obj;
        SpaceObjRot rotatable;
        SpaceObjRotImp impactable;
        SpaceObjRotImpTarg targetable;
        Resource resource;
    };

    // put Gas Cloud pecific stuff here only
    GasCloudType gascloudtype;  // gascloud specific information
    real32 scaling;
} GasCloud;

/*-----------------------------------------------------------------------------
    Dust Cloud
-----------------------------------------------------------------------------*/

typedef struct DustCloud
{
    union {
        struct {
            SpaceObjHeader header;
            // Reinterprets `SpaceObj.staticinfo`
            DustCloudStaticInfo* staticinfo;
        };
        SpaceObj obj;
        SpaceObjRot rotatable;
        SpaceObjRotImp impactable;
        SpaceObjRotImpTarg targetable;
        Resource resource;
    };

    // Dust Cloud specific stuff
    DustCloudType dustcloudtype;
    real32 scaling;
    cloudSystem* stub;
} DustCloud;

#define MAX_NUM_DROP    5        //maximum number of targets that can be dropped at a motherships/carrier


#ifdef GOD_LIKE_SYNC_CHECKING

typedef struct
{
    udword univUpdateCounterValue;
    udword numShips;    //number of ships in this snap shot
    Ship ship[1];       //dynamic array of ships
}UniverseSnapShot;

typedef struct
{
    char playerName[20];    //name of curplayer
    sdword windowSize;      //number of universeSnapShots in this file
    UniverseSnapShot *universeSnapShot[1]; //array of snapshots
}GodWritePacketHeader;

extern GodWritePacketHeader *godUniverseSnapShotHeader;

#define sizeofGodHeader(n)      sizeof(GodWritePacketHeader) + sizeof(UniverseSnapShot *)*(n-1)
#define sizeofSnapShot(n)       sizeof(UniverseSnapShot)+ sizeof(Ship)*(n)



#endif

/*=============================================================================
    Macros:
=============================================================================*/

#define sizeofGunInfo(n) (sizeof(GunInfo) + (n-1)*sizeof(Gun))
#define sizeofGunStaticInfo(n) (sizeof(GunStaticInfo) + (n-1)*sizeof(GunStatic))
#define sizeofDockStaticInfo(n) (sizeof(DockStaticInfo) + (n-1)*sizeof(DockStaticPoint))
#define sizeofNavLightStaticInfo(n) (sizeof(NAVLightStaticInfo) + (n-1)*sizeof(NAVLightStatic))
#define sizeofDockInfo(n) (sizeof(DockInfo) + (n-1)*sizeof(DockPoint))
#define sizeofNavLightInfo(n) (sizeof(NAVLightInfo) + (n-1)*sizeof(NAVLight))

//#define isCapitalShip(ship) (((ShipStaticInfo *)((ship)##->staticinfo))->shipclass <= CLASS_Frigate)
//#define isCapitalShipStatic(shipstatic) (shipstatic##->shipclass <= CLASS_Frigate)
#define isCapitalShip(ship) (((ShipStaticInfo *)((ship)->staticinfo))->shipIsCapital)
#define isCapitalShipStatic(shipstatic) ((shipstatic)->shipIsCapital)

#define isShipOfClass(ship,checkclass) (((ShipStaticInfo *)((ship)->staticinfo))->shipclass == (checkclass))

/*=============================================================================
    Support Functions:
=============================================================================*/

#endif //___SPACEOBJ

