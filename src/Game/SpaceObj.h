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
#include "Timer.h"
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

// CAT (M2-R4 combat): the exe's Gun is 0x88 bytes (HW1 0x84): univCreateShip 0x00542320 allocates numGuns * 0x88 + 4,
//      and every field after the new word at +0x00 is HW1's, 4 bytes on (gunstatic +0x84)
typedef struct
{
    sdword catWeaponType;       // CAT: exe Gun +0x00, the gun's current WEAPON_*: gunstatic->startUpgradeLevel at
                                //   creation (cat_shipUpgradesInit 0x005140a0), raised to WEAPON_ENERGY by
                                //   cat_shipApplyUpgrades 0x00514490; == endUpgradeLevel selects the second fire mode

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

// CAT (M2-R4 combat): no HW1 salvage statics (SalvageStaticPoint/SalvageStaticInfo/SalvagePoint/SalvageInfo,
//      SALVAGE_MAX_CAPTURABLE): Cataclysm salvages at the target's DPT_Salvage/DPT_SalvageLatch dock points
//      (Salvage.c), and cataclysm.exe has neither the NUMBER_OF_SALVAGE_POINTS/SalvagePoint keys nor a mex salvage reader.

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

// CAT (M2-R4 combat, M2-R5 dock): Cataclysm's per-ship dock info is 0x24 + 4n bytes (cat_dockInitShipDockInfo
//      0x00463bb0 allocates numDockPoints x 4 + 0x24 and zeroes it): a count of the ships holding a point of each
//      dock point type at +0x00 (0x23 bytes, ThisShipCanDockWith 0x00464290 tests it against
//      DockStaticInfo.numOfType), then one busy word per dock point at +0x24 (dockReserveDockPoint 0x00464610 sets
//      it, RemoveShipFromDocking 0x00465360 clears it). HW1's busyness and per-point static pointers are gone; the
//      point count is the static's.
typedef struct
{
    ubyte catNumBusyOfType[NUM_DOCKPOINT_TYPES];    // +0x00 exe DockInfo+0x00
    ubyte pad23;
    sdword thisDockBusy[1];                         // +0x24 one per dock point
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
//      block: count at +0, objects from +8, i.e. up to 1000; host == NULL for a pure host); Clamp.c keeps it.
// CAT (M2-R4 combat): the host's list of clamped objects (clampObjToObj 0x004346e0: memAlloc(0xFA8), count at +0,
//      objects from +8; the head is a SelectCommand's, so the readers cast it to one)
typedef struct CatClampedList
{
    sdword numShips;
    real32 timeLastStatus;
    struct Ship *ShipPtr[1000];
} CatClampedList;
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


//info for SpaceObjRotImpTarg objects that was once
//static, but now needs to be non-static
typedef struct
{
    real32 thruststrength[NUM_TRANS_DEGOFFREEDOM];
    real32 rotstrength[NUM_ROT_DEGOFFREEDOM];
    real32 turnspeed[NUM_TURN_TYPES];
    // CAT (M2-R5 layout): a 16th float: the exe's NonStats is 0x40 bytes (Ship +0x17C..+0x1BC, clampInfo at +0x1BC).
    //      univCreateShip 0x00542320 and univAddMissile 0x00540c70 store staticheader.maxvelocity here (0x005425f3,
    //      0x00540e6f); the speed code reads the static value through cat_shipGetSpeedMultiplier, so this copy is
    //      kept for the layout. Name chosen here.
    real32 catMaxVelocity;
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
    bool32 (*CustShipLaunchCommand)(struct Ship *ship, struct Ship *receiverShip);                 /* slot 23, CAT new: clLaunchShip 0x0044a220 passes (ship, receiverShip) [audit: M2-R5 layout-dock] */
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
    // CAT (M2-R5 dock): HW1's per-type dock callbacks ShipXDocksAtShipY / LaunchShipXFromShipY: the two slots are in the
    //      exe's layout but InitStatShipInfo 0x0053bab0 never sets them and nothing calls through them (the dock state
    //      machine cat_dockShipDocksAtShip 0x00465860 is generic). Kept as dormant words for the layout.
    void *catStaticDormant400;                  /*+0x400*/
    void *catStaticDormant404;                  /*+0x404*/

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
_Static_assert(sizeof(Gun) == 0x88, "Gun is 0x88 bytes in cataclysm.exe (univCreateShip 0x00542320)");
CAT_ASSERT_OFFSET(Gun, gunstatic, 0x84);
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
CAT_ASSERT_OFFSET(ShipStaticInfo, catStaticDormant400, 0x400);
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
    // CAT: HW1's asteroidCanShrink/asteroidCanBreak/asteroidMinShrink/breakinto[] are gone: no Cataclysm key sets
    //      them (asteroid key table 0x008b3008) and BreakAsteroidUp 0x004f1580 breaks by the .shp pieces above
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
_Static_assert(sizeof(AsteroidStaticInfo) == 0xC8, "AsteroidStaticInfo is 0xC8 bytes in cataclysm.exe");
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

// CAT (M2-R5 layout): HW1's SlaveInfo (ship slavery: masters, slaves, SF_*) is gone. The exe has no slavery
//      (SOF_Slaveable has no bit, decision 15; Dock.c's dockMakeMaster family has no exe counterpart) and its Ship
//      has no slavelink/slaveinfo slots.

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
    // CAT: two new header dwords before staticinfo (the exe's objects have staticinfo at +0x20, HW1 +0x18).
    //      +0x18 is one flag word for every object type (M2-R5 layout: it was Ship.catShipFlags for ships and
    //      catHeader18 otherwise, bridged by CAT_OBJ_FLAGS18): the CAT_SHIPFLAG_* bits (Mimic, KasDerelict, NoTactics,
    //      Orphaned) and CAT_OBJFLAG18_RemoveReferences 0x100 (cat_univRemoveObjReferencesFromShips 0x00543f10 on any
    //      object); etgEffectCreate 0x004709ed clears it. +0x1c is set to 1 by the ETG callable
    //      omitParentShipFromRenderList (cat_etgParentShipOmitFromRenderList 0x00470190) and cleared for
    //      world-rendered effects (0x00470ba3); its reader is the render list builder (stage 3)
    udword catFlags18;
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
    // CAT (M2-R5 layout): the exe's mission attribute word is a dword at +0x38 (kasfShipsAttributesSet 0x004933d0
    //      stores the whole dword; the killer-collision bit 0x200, the blast bits 0x10000/0x20000 of
    //      cat_blastAffectShip 0x00515d30 and the crystal option read it), with HW1's uword slot at +0x36 unused and
    //      the attributes parameter a sword at +0x3C (univAddAsteroid 0x00540a30 clears the word there);
    //      cameraDistanceSquared follows at +0x40 (CET)
    ubyte catPad36[2];               // +0x36: unused (HW1's uword attributes slot)
    udword attributes;               // +0x38: settable attributes from mission editor (CAT_ATTRIBUTES_* / ATTRIBUTES_*)
    sword attributesParam;           // +0x3C: parameter for attributes, set from mission editor
    ubyte attributesPad[2];          // +0x3E: leftover, free space
    real32 cameraDistanceSquared;    // +0x40: distance to camera (for sound, level of detail)
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
    // CAT (M2-R5 layout): four more dwords before rotinfo, which the exe keeps at +0x98 (cat_dockShipDocksAtShip
    //      0x00465860 and TurnHarvestEffectOn 0x004f1ac0 pass ship+0x98 as the coordinate system; collInfo then starts
    //      at +0xD4, impactablelink at +0x160 and health at +0x170). Every creator zeroes them with the object
    //      (memAlloc + clear). [M2-R5 render: it is a list node: rndMainViewRenderFunction 0x00576b7b puts every
    //      visible object on a frame-local list through it (lea eax,[esi+0x88]; listAddNode). Name chosen here.]
    Node catVisibleLink;             // exe +0x88 .. +0x94
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

// CAT (M2-R3 selection): number of catDisableReason slots (cat_shipSetDisableReason 0x005b3fe0 scans 13 dwords)
#define CAT_NUM_DISABLE_REASONS                     13

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

    // CAT (M2-R4 combat): no HW1 salvageInfo pointer: in cataclysm.exe the clamp info (+0x1BC) is followed directly by
    //      aistate (+0x1C0) and dockingship (+0x1C4) (RemoveShipFromDocking 0x00465360, collCheckShipShipColl
    //      0x0043ade0); Cataclysm salvages at DPT_Salvage dock points (Salvage.c)

    //must come next to be compatible with Derelict in aiship stuff
    ubyte aistate;                  // +0x1C0 AI state variable
    ubyte pad1[2];
    sbyte putOnDoor;                // +0x1C3
    struct Ship *dockingship;       // +0x1C4 if non-NULL, indicates no collisions should occur with this ship
    udword specialFlags;            // +0x1C8 general purpose ship flags, see above for defines
    udword specialFlags2;           // +0x1CC more general purpose ship flags, see above for defines
    // CAT (M2-R5 layout): the exe's prefix between specialFlags2 and the per-player salvage counts (+0x1D0..+0x214),
    //      shared with derelicts (univAddDerelictByStatInfo 0x00541000 clears the same bytes). The fields were
    //      appended to Ship by name in M2-R3/R4; they move here at the exe's offsets.
    bool32 catRetreating;           // +0x1D0: set to 1 by battlePingEvaluate 0x00428b70 when the retreat chatter
                                    //   (event 0x18) plays for a ship below batRetreatModifier x the ping's strength;
                                    //   cleared by every new order (PrepareOneShipForCommand 0x00441890, clHalt,
                                    //   ChangeOrderToMove/Attack/Protect/HyperSpace/PassiveAttack, cat_clLink/Mimic/
                                    //   PointOrder, RemoveShipFromBeingTargeted). Name chosen here.
    bool32 catResourceFinished;     // +0x1D4: the resource it harvested is used up (CollectResources 0x004f17a0
                                    //   sets, the AI's aimProcessResourceVolume reads and clears); HW1's specialFlags bit
    bool32 catResourcing;           // +0x1D8: harvesting for the AI (CollectResources clears, aimProcessResourceVolume
                                    //   sets, aiuShipIsResourcingOrSomething reads); HW1's specialFlags bit
    sdword catDisableReason[CAT_NUM_DISABLE_REASONS];   // +0x1DC: cat_shipSetDisableReason 0x005b3fe0 sets [reason]
    bool32 catDisabled;             // +0x210: TRUE while any catDisableReason[] is nonzero (cat_shipSetDisableReason
                                    //   0x005b3fe0, cat_shipSetHelpless 0x005b3bd0, cat_shipClearHelpless 0x005b3c80,
                                    //   cat_empShockShip 0x005b3900, cat_shipUpdateDrift 0x005b3d60 recompute it)
    sdword salvageNumTagged[MAX_MULTIPLAYER_PLAYERS];   // +0x214: number of salvagers vying for the ship, per player
                                    //   (univCreateShip 0x00542320 zeroes 8 dwords at +0x214; Salvage.c reads them)
    NAVLightInfo *navLightInfo;     // +0x234: InitializeNavLights 0x00542040 stores the block at +0x234 of a ship or a
                                    //   derelict (both creators call it)
} ShipOrDerelict;   // ships, derelicts: 0x238 bytes

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

// CAT (M2-R5 dock): cataclysm.exe's dock variables are five words at Ship+0x494 (the cone state machine of
//      cat_dockShipDocksAtShip 0x00465860 keeps nothing else per ship). HW1's destination, dockstate2/3,
//      reserveddocking, dockstaticpoint, customdockinfo and launchpoints are gone with its per-type dock paths.
//      Names chosen here where the exe's are unknown.
typedef struct
{
    struct Ship *dockship;          // +0x494 the host (InitializeDockVars 0x00463c40; NULL when not docking)
    real32 catDockDistance;         // +0x498 the dock point's distance for the fly-in: cat_dockFindNearestDockPoint
                                    //   0x00464660 stores the ship's distance clamped to [mindist, maxdist],
                                    //   cat_dockFindLaunchPoint 0x004649b0 the point's mindist
    sdword dockstate;               // +0x49C CAT_DOCKSTATE_* (Dock.h)
    sdword catDockPointIndex;       // +0x4A0 the index of the dock point held on dockship, or -1 (univCreateShip
                                    //   0x00542320 and InitializeDockVars set -1, the point finders and
                                    //   dockReserveDockPoint 0x00464610 set it, RemoveShipFromDocking 0x00465360 frees it;
                                    //   the salvage depots use it for towed pods)
    real32 catDockTime;             // +0x4A4 universe time the docked/latched ship may leave (dock states 8, 9, 0xD, 0xE)
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

// CAT (M2-R4 command, subsystem `flags`): Ship.specialFlags (exe Ship+0x1C8) and Ship.specialFlags2 (exe Ship+0x1CC)
//      at cataclysm.exe's bits. Method: tools/m2_specialflags_scan.py lists every masked access to the two words in
//      the exe's disassembly (immediates, register immediates, byte accesses, shifted loads) per exe function and joins
//      it with the HW1 names the same function uses in GoK; each HW1 flag below sits where its HW1 functions' exe bodies
//      test or set it, each new flag where its setter (mostly a KAS function) puts it. Cataclysm dropped HW1's AI
//      resourcing pair (14/15) and three HW1 salvage flags (21/23/24), added a KAS repair modifier at 13, and moved HW1's
//      specialFlags2 bits 0-3 into specialFlags 27-30; of HW1's specialFlags2 it keeps only DontPlowThroughEnemyShips
//      (now bit 0), the rest of that word is Cataclysm's. Per-bit evidence: docs/m2-r4-command.md "flags".
//      HW1 flags with no exe bit are CAT-DORMANT and defined 0, as the retired SOF_* flags: setting one does nothing
//      and testing one is FALSE, which is what the exe does at those HW1 sites (none of its bodies touches such a bit).
//      Their remaining readers belong to the owners named per flag. Bits of the exe words with no access found:
//      specialFlags 4, 5, 9, 10, 11, 26, 31; specialFlags2 1, 2, 5, 7, 8, 9, 10, 30, 31.
#define SPECIAL_StopForResearchDocking              0x00000001  // CAT: kept: cat_dockShipDocksAtShip 0x00465860 tests it (no exe setter found)
#define SPECIAL_Kamikaze                            0x00000002  // processAttackToDo, delegateCommand, tacticsGetShipsMaxVelocity 0x00529450
#define SPECIAL_BrokenFormation                     0x00000004  // processFormationToDo, AttackCleanup; the Mimic/MCV/P1StandardCorvette/CruiseMissile attacks set 0x6 (Kamikaze|BrokenFormation)
#define SPECIAL_KamikazeCrazyFast                   0x00000008  // doKamikazeAttack 0x005b1a50, tacticsGetShipsMaxVelocity
#define SPECIAL_SpeedBurst                          0x00000000  // CAT-DORMANT: retired (HW1 0x10): no speed burst in the exe (tacticsGetShipsMaxVelocity tests only 0x2/0x8/0x100); Tactics.c readers: M2-R5 command-tactics
#define SPECIAL_SpeedBurstCooling                   0x00000000  // CAT-DORMANT: retired (HW1 0x20), as SPECIAL_SpeedBurst
#define SPECIAL_BurstFiring                         0x00000040  // doBurstFire 0x005b5100, cat_gunCreateBullet 0x0048bb00
#define SPECIAL_SinglePlayerWithinHyperSpaceRange   0x00000080  // CalculateRollCall, TellHyperspacingShipsToReady 0x00517770, spGoNowHyperspaceCB 0x005173a0
#define SPECIAL_SinglePlayerLimitSpeed              0x00000100  // kasfShipsSetMaxVelocity 0x004938f0, kasfTeamSetMaxVelocity 0x00493860, tacticsGetShipsMaxVelocity
#define SPECIAL_EXITING_DOOR                        0x00000000  // CAT-DORMANT: retired (HW1 0x800): no exe access; its HW1 Dock.c readers (the carrier/mothership door paths) went with the M2-R5 dock rewrite [audit: M2-R5 layout-dock]
// CAT (M2-R4 combat): HW1's salvage flags (BUSY_BECAUSE_SALVAGE_DOCKING 0x200, Harvested 0x400, IsASalvager
//      0x100000, SalvagerHasSomethingAttachedAndIsDoingSomethingElse 0x200000, SalvagedTargetGoingIntoDockWithShip
//      0x800000, SalvageTakingHomeTechnology 0x1000000) were retired by M2-R4 command and are gone with HW1's salvage
//      code and its readers; Cataclysm's salvager mark is SPECIAL_CatSalvager.
#define SPECIAL_STAY_TILL_EXPLICITLAUNCH            0x00001000  // InitializeDockVars 0x00463c40, ShipHasToLaunch 0x0044a780, RemoveShipFromLaunching 0x00465590
#define SPECIAL_CatKasSpecialRepairModifier         0x00002000  // CAT: new: kasfShipsSetRepairFactor 0x00493a10 / kasfShipsClearRepairFactor 0x00493a60
#define SPECIAL_KasSpecialDamageModifier            0x00004000  // CAT: was 0x2000. kasfShipsSetDamageFactor 0x00493980, gunShoot 0x0048b5b0, missileShoot 0x0048aac0
#define SPECIAL_Finished_Resource                   0x00000000  // CAT-DORMANT: retired (HW1 0x4000, HW1 AI resourcing): Cataclysm keeps Ship.catResourceFinished / catResourcing (ResCollect.c, M2-R4 combat); the AIMoves.c/AIUtilities.c readers go with stage 4
#define SPECIAL_Resourcing                          0x00000000  // CAT-DORMANT: retired (HW1 0x8000), as SPECIAL_Finished_Resource
#define SPECIAL_SinglePlayerInParade                0x00008000  // CAT: was 0x10000. spGoNowHyperspaceCB, TellHyperspacingShipsToReady set 0x8080 where HW1 sets WithinHyperSpaceRange|InParade
#define SPECIAL_rowGettingOutOfWay                  0x00010000  // CAT: was 0x20000. rowFlyShipOutOfWay 0x00414280, PrepareShipsForCommand 0x00441000 and 25 more
#define SPECIAL_ATTACKMOVECANCEL                    0x00020000  // CAT: was 0x40000. clProcess 0x0044b950 sets, ChangeOrderToMove 0x004460d0 tests
#define SPECIAL_FriendlyStatus                      0x00040000  // CAT: was 0x80000. kasfShipsSetNonRetaliation 0x00495da0, selShipUnderMouse 0x00506550, MakeTargetsOnlyEnemyShips 0x00512970; CrystalInit, SlipgateInit set it
#define SPECIAL_CatSalvager                         0x00080000  // CAT: Cataclysm's salvager mark: cat_salvageInit 0x004f9010 sets it; collCheckShipShipColl 0x0043ade0, aishipFlyToPointAvoidingObjsFunc 0x00414610, selStatusDraw0 0x00507f10 test it where HW1 tests SPECIAL_IsASalvager
#define SPECIAL_AttackFromAbove                     0x00100000  // CAT: was 0x400000. attackSideStep 0x00421fa0; RemoveShipFromAttacking 0x004477c0 clears 0x100006
#define SPECIAL_Hyperspacing                        0x00200000  // CAT: was 0x2000000. CalculateHyperspaceTimeForShip 0x005178a0 sets, UpdateArrivingShip 0x00518170 clears, attackPassive 0x00421c30 tests
#define SPECIAL_ForcedAttackStatus                  0x00400000  // CAT: was 0x4000000. kasfForceCombatStatus 0x00493d70; cat_univShipSelfRepair 0x005433d0 reads it as "in combat"
#define SPECIAL_LaunchedFromKas                     0x00800000  // CAT: was 0x10000000. kasfLaunch 0x004947c0 sets 0x1800000, aiplayerShipLaunchedCallback 0x00411390 tests
#define SPECIAL_KasCheckDoneLaunching               0x01000000  // CAT: was 0x20000000. kasfLaunch sets, kasfTeamFinishedLaunching 0x004948a0 tests
#define SPECIAL_Launching                           0x02000000  // CAT: was 0x40000000. clLaunchShip 0x0044a220 tests (no exe setter found)
#define SPECIAL_ParadeNeedTomoveCloser              0x00000000  // CAT-DORMANT: retired (HW1 0x80000000): processMilitaryParadeToDo 0x00485280 touches neither word
#define SPECIAL_SalvageTargetAlreadyDriven          0x08000000  // CAT: was specialFlags2 0x1. univSetupShipForControl 0x0054a230 clears it first thing, where HW1 clears SPECIAL_2_SalvageTargetAlreadyDriven
#define SPECIAL_ShipInGravwell                      0x10000000  // CAT: was specialFlags2 0x2. GravWellGeneratorAddObj 0x005b4570 sets; gunShoot, univBulletCollidedWithTarget test
#define SPECIAL_InUnitCaps                          0x20000000  // CAT: was specialFlags2 0x4. unitCapCreateShip 0x0053eab0 sets, unitCapDeleteShip 0x0053ec00 tests/clears
#define SPECIAL_CircularGuard                       0x40000000  // CAT: was specialFlags2 0x8. clProtect 0x0044af20 sets/clears; protectShip 0x004411c0, protectShipsAvg test

//////////SPECIAL_2 Flags
#define SPECIAL_2_DontPlowThroughEnemyShips         0x00000001  // CAT: was 0x20. aishipFlyToPointAvoidingObjsFunc 0x00414610 tests it (0x004151c5) where HW1 does
#define SPECIAL_2_CatBeingBuilt                     0x00000008  // CAT: built externally: univCreateShip 0x00542320 sets, cat_clExternalBuildUpdate 0x0044dfc0 and cat_shipUpdateExternalBuildAndAI 0x00543f10 clear, kasfShipUnderConstruction 0x00493400 returns it; about 60 exe readers
#define SPECIAL_2_CatKasAttackCapable               0x00000010  // CAT: kasfShipsAttackCapable 0x00498100 writes (bCapable & 1) << 4 (no reader found)
#define SPECIAL_2_CatNoAvoidance                    0x00000040  // CAT: aishipFlyToPointAvoidingObjsFunc skips avoiding when set; cat_dockShipDocksAtShip, kasfMakeShipCamera 0x00498290, cat_leechProcess 0x005b6f30, cat_salvageSpecialTarget 0x004faad0, RammingFrigateHouseKeep set it; RemoveShipFromDocking/Launching, LaunchCleanup, cat_clLinkCleanup 0x00444060 clear it (name chosen here)
#define SPECIAL_2_CatHarvesting                     0x00000800  // CAT: processCollectResource 0x004f2130 and cat_rescollectStopHarvest 0x004f2450 clear it (no setter found; name chosen here)
#define SPECIAL_2_CatSentinelFormation              0x00001000  // CAT: SentinelFormationCommand 0x005c6df0 sets/clears (name chosen here)
#define SPECIAL_2_CatBeingSalvaged                  0x00002000  // CAT: cat_salvageIsBeingSalvaged 0x004f7c00 returns it
// CAT (M2-R3 ships-a): the same bit is set by cat_salvageDepotInit 0x004fc7c0 on every salvage depot (carriers,
//      motherships, processors, bHeavyCruiser), so it marks a ship as a depot; this name says so.
#define SPECIAL_2_CatSalvageDepot                   SPECIAL_2_CatBeingSalvaged
#define SPECIAL_2_CatParalyzed                      0x00004000  // CAT: kasfShipsParalyze 0x00493420 writes (paralyze & 1) << 14; cat_empCanAffectShip 0x005add30, ACVSpecialTarget test it
#define SPECIAL_2_CatBeastInfectionUnlimited        0x00008000  // CAT: kasfShipsSetBeastInfectionUnlimited 0x00498a60 sets, cat_beastWeaponInit 0x0042a040 clears
#define SPECIAL_2_CatBeastified                     0x00010000  // CAT: cat_univBeastInfectionUpdate 0x00543860 sets; kasfShipsSelectBeastified 0x00498ae0 selects by it
#define SPECIAL_2_CatSalvaged                       0x00020000  // CAT: cat_salvageDepotProcessGeneric 0x004f9500 sets; kasfShipsSelectSalvaged 0x00498b30 selects by it
#define SPECIAL_2_CatSalvagedFlag18                 0x00040000  // CAT: cat_salvageDepotProcessGeneric writes it with SPECIAL_2_CatSalvaged (set when the depot's +0x32C block word +0x24 is 0; meaning not pinned)
#define SPECIAL_2_CatExternalBuildTimer             0x00080000  // CAT: cat_clStartExternalBuildTimer 0x0044aa30 sets, cat_shipUpdateExternalBuildAndAI clears, ModuleShipDied tests
// CAT (M2-R3 ships-a): bit 20 (0x100000): ships with it get no mothership health warning (MothershipHouseKeep
//      0x005bcad0), no battle pings or death screams (battlePingEvaluate 0x00424110), no support-unit accounting
//      (cat_suShipAdded 0x00459a90) and special docking rules (ThisShipCanDockWith 0x00464290). M2-R4: its setter is
//      cat_shipSetSUExempt 0x005b4020, (exempt & 1) << 20.
#define SPECIAL_2_CatExempt20                       0x00100000
#define SPECIAL_2_CatKasBulimia                     0x00200000  // CAT: kasfShipsSetBulimia 0x0049a030 writes it; CollectResources 0x004f17a0 tests
#define SPECIAL_2_CatKasNaggarok                    0x00400000  // CAT: kasfEnableNaggarok 0x0049a220 writes it; cat_rkoCanTarget 0x005c4050, RKOHouseKeep test
#define SPECIAL_2_CatBeastWeaponAutoAttack          0x00800000  // CAT: kasfEnableBeastWeaponAutoAttack 0x0049a400 writes it; cat_shipUpdateExternalBuildAndAI tests
#define SPECIAL_2_CatInvincibleInWidescreen         0x01000000  // CAT: kasfShipsSetInvincibleInWidescreen 0x0049abd0 writes it; ApplyDamageToTarget 0x00546250 tests
#define SPECIAL_2_CatSalvageTargeted                0x02000000  // CAT: cat_salvageSpecialTarget sets, cat_salvageDetach 0x004f88b0 clears; cat_shipIsTargetableBy 0x0052a080, cat_empCanAffectShip test (name chosen here)
#define SPECIAL_2_CatResourcing                     0x04000000  // CAT: processCollectResource sets, rescollectFindNearestResource 0x004f11f0 tests/clears (name chosen here)
#define SPECIAL_2_CatStayMimicked                   0x08000000  // CAT: kasfSetStayMimicked 0x0049a850 writes (b & 1) << 27; cat_mimicCleanup 0x005b8ea0 tests
#define SPECIAL_2_CatSUProvided                     0x10000000  // CAT: cat_suShipProvidesSU 0x00459a60 / cat_suShipAdded set, cat_suShipRemoved 0x00459ae0 tests/clears (name chosen here)
#define SPECIAL_2_CatSlipgate                       0x20000000  // CAT: SlipgateInit 0x005c7250 sets; ThisShipCanDockWith tests it on the dock target (name chosen here)
// CAT-DORMANT: HW1 specialFlags2 bits with no exe counterpart (retired, defined 0): NoMoreDockingAllowed (HW1 0x10,
//      ShipDocksAtJunkYardHQ, a dropped type: M2-R4 #4). HW1's salvage-only BusiedDockPoint (0x40) and
//      DisabledForever (0x80) are gone with HW1's salvage code (M2-R4 combat; the exe's MothershipDied 0x005bb9c0
//      tests SPECIAL_2_CatBeingBuilt where HW1 tests DisabledForever on its drop targets).
#define SPECIAL_2_NoMoreDockingAllowed              0x00000000

// CAT (M2-R4 command): static checks. The live flags of each word are distinct bits (their OR equals their sum) and
//      cover exactly the bits the exe uses; the values the M2-R3 ports quoted are these.
#define CAT_SPECIAL_LIVE_FLAGS \
    SPECIAL_StopForResearchDocking, SPECIAL_Kamikaze, SPECIAL_BrokenFormation, SPECIAL_KamikazeCrazyFast, \
    SPECIAL_BurstFiring, SPECIAL_SinglePlayerWithinHyperSpaceRange, SPECIAL_SinglePlayerLimitSpeed, \
    SPECIAL_STAY_TILL_EXPLICITLAUNCH, SPECIAL_CatKasSpecialRepairModifier, SPECIAL_KasSpecialDamageModifier, \
    SPECIAL_SinglePlayerInParade, SPECIAL_rowGettingOutOfWay, SPECIAL_ATTACKMOVECANCEL, SPECIAL_FriendlyStatus, \
    SPECIAL_CatSalvager, SPECIAL_AttackFromAbove, SPECIAL_Hyperspacing, SPECIAL_ForcedAttackStatus, \
    SPECIAL_LaunchedFromKas, SPECIAL_KasCheckDoneLaunching, SPECIAL_Launching, SPECIAL_SalvageTargetAlreadyDriven, \
    SPECIAL_ShipInGravwell, SPECIAL_InUnitCaps, SPECIAL_CircularGuard
#define CAT_SPECIAL_2_LIVE_FLAGS \
    SPECIAL_2_DontPlowThroughEnemyShips, SPECIAL_2_CatBeingBuilt, SPECIAL_2_CatKasAttackCapable, \
    SPECIAL_2_CatNoAvoidance, SPECIAL_2_CatHarvesting, SPECIAL_2_CatSentinelFormation, SPECIAL_2_CatBeingSalvaged, \
    SPECIAL_2_CatParalyzed, SPECIAL_2_CatBeastInfectionUnlimited, SPECIAL_2_CatBeastified, SPECIAL_2_CatSalvaged, \
    SPECIAL_2_CatSalvagedFlag18, SPECIAL_2_CatExternalBuildTimer, SPECIAL_2_CatExempt20, SPECIAL_2_CatKasBulimia, \
    SPECIAL_2_CatKasNaggarok, SPECIAL_2_CatBeastWeaponAutoAttack, SPECIAL_2_CatInvincibleInWidescreen, \
    SPECIAL_2_CatSalvageTargeted, SPECIAL_2_CatResourcing, SPECIAL_2_CatStayMimicked, SPECIAL_2_CatSUProvided, \
    SPECIAL_2_CatSlipgate
#define CAT_FLAGS_OR25(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y) \
    ((unsigned long long)(a)|(b)|(c)|(d)|(e)|(f)|(g)|(h)|(i)|(j)|(k)|(l)|(m)|(n)|(o)|(p)|(q)|(r)|(s)|(t)|(u)|(v)|(w)|(x)|(y))
#define CAT_FLAGS_SUM25(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y) \
    ((unsigned long long)(a)+(b)+(c)+(d)+(e)+(f)+(g)+(h)+(i)+(j)+(k)+(l)+(m)+(n)+(o)+(p)+(q)+(r)+(s)+(t)+(u)+(v)+(w)+(x)+(y))
#define CAT_FLAGS_OR23(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w) \
    ((unsigned long long)(a)|(b)|(c)|(d)|(e)|(f)|(g)|(h)|(i)|(j)|(k)|(l)|(m)|(n)|(o)|(p)|(q)|(r)|(s)|(t)|(u)|(v)|(w))
#define CAT_FLAGS_SUM23(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w) \
    ((unsigned long long)(a)+(b)+(c)+(d)+(e)+(f)+(g)+(h)+(i)+(j)+(k)+(l)+(m)+(n)+(o)+(p)+(q)+(r)+(s)+(t)+(u)+(v)+(w))
#define CAT_FLAGS_APPLY(m, ...) m(__VA_ARGS__)
_Static_assert(CAT_FLAGS_APPLY(CAT_FLAGS_OR25, CAT_SPECIAL_LIVE_FLAGS) ==
               CAT_FLAGS_APPLY(CAT_FLAGS_SUM25, CAT_SPECIAL_LIVE_FLAGS), "two specialFlags share a bit");
_Static_assert(CAT_FLAGS_APPLY(CAT_FLAGS_OR25, CAT_SPECIAL_LIVE_FLAGS) == 0x7BFFF1CFull,
               "specialFlags: the 25 bits cataclysm.exe uses (all but 4, 5, 9, 10, 11, 26, 31)");
_Static_assert(CAT_FLAGS_APPLY(CAT_FLAGS_OR23, CAT_SPECIAL_2_LIVE_FLAGS) ==
               CAT_FLAGS_APPLY(CAT_FLAGS_SUM23, CAT_SPECIAL_2_LIVE_FLAGS), "two specialFlags2 flags share a bit");
_Static_assert(CAT_FLAGS_APPLY(CAT_FLAGS_OR23, CAT_SPECIAL_2_LIVE_FLAGS) == 0x3FFFF859ull,
               "specialFlags2: the 23 bits cataclysm.exe uses (0, 3, 4, 6, 11-29)");
_Static_assert(SPECIAL_FriendlyStatus == 0x40000 && SPECIAL_Hyperspacing == 0x200000 &&
               SPECIAL_CircularGuard == 0x40000000 && SPECIAL_CatSalvager == 0x80000 &&
               (SPECIAL_Kamikaze | SPECIAL_BrokenFormation) == 0x6, "specialFlags at cataclysm.exe's bits");
_Static_assert(SPECIAL_2_CatBeingBuilt == 0x8 && SPECIAL_2_CatBeingSalvaged == 0x2000 &&
               SPECIAL_2_CatBeastInfectionUnlimited == 0x8000 && SPECIAL_2_CatExempt20 == 0x100000,
               "specialFlags2 at cataclysm.exe's bits");

// CAT (M2-R3 selection): bits of the +0x18 word (SpaceObjHeader.catFlags18; exe Ship+0x18)
#define CAT_SHIPFLAG_Mimic                          BIT3    // set by MimicInit 0x005b8360, MCVInit 0x005b8130, P1StandardCorvetteInit 0x005be660
#define CAT_SHIPFLAG_KasDerelict                    BIT4    // kasfShipsSetDerelict 0x00498510
#define CAT_SHIPFLAG_NoTactics                      BIT1    // cat_shipCanSetTactics 0x005135e0 refuses it (setter not located)
// CAT (M2-R3 ships-a): set on the component-class ships clamped to a carrier or mothership when the host dies
//      (CarrierDied 0x005af3f0, MothershipHouseKeep 0x005bcad0); read by cat_salvageCheckTarget 0x004f7eb0 and
//      cat_kamikazeSteer 0x005b16f0. Name chosen here.
#define CAT_SHIPFLAG_Orphaned                       BIT2
// CAT (M2-R5 dock): set by InitializeDockVars 0x00463c40 for the launch purpose (2) with dock state 10, cleared by
//      RemoveShipFromLaunching 0x00465590, LaunchCleanup 0x004679b0 and processLaunchShipToDo 0x00466b00. Name chosen.
#define CAT_SHIPFLAG_Launching                      BIT0
// CAT (M2-R5 command-tactics): clScuttle 0x0044a150 does not scuttle a ship with this bit whose infection timer
//      (catBeastConvertTime, Ship+0x4E4) runs; no setter was located (a register-valued store). Name chosen here.
#define CAT_SHIPFLAG_Bit7                           BIT7


// CAT (M2-R3 selection): Ship.catUpgradeFlags bits the selection code tests (names chosen here from their users)
#define CAT_UPGRADE_Link                            0x00000008  // cat_selComputeTargetActions 0x00513a00 (the link action)
#define CAT_UPGRADE_Leech                           0x00000040  // cat_shipCanLeech 0x005136a0
#define CAT_UPGRADE_Ability1                        0x00001000  // cat_selMakeShipsHaveAbilityUpgrade 0x00512330
#define CAT_UPGRADE_Ability2                        0x00002000
#define CAT_UPGRADE_Ability3                        0x00004000
#define CAT_UPGRADE_Repair                          0x00040000  // cat_shipCanRepair 0x005138b0
#define CAT_UPGRADE_Salvage                         0x00080000  // cat_shipCanSalvage 0x005138f0
#define CAT_UPGRADE_AcolyteLinkFire                 0x04000000  // cat_acolyteCanFireLinkedWeapon 0x005ada50
// CAT (M2-R4 ships-b): the upgrade bits the Cataclysm-only ships test (names chosen here from their users)
#define CAT_UPGRADE_MimicSmall                      0x00000010  // cat_mimicBecomeShipType 0x005b88f0: an sMCV may pose as a fighter/micro (it unlinks); names chosen here
#define CAT_UPGRADE_Mimic                           0x00000020  // cat_mimicCommand 0x005b8d70, MimicSpecialTarget 0x005b8dd0
#define CAT_UPGRADE_MimicRam                        0x00000080  // cat_shipGetRamDamageScale 0x005b9980
#define CAT_UPGRADE_ACVEMP                          0x00000200  // ACVSpecialActivate 0x005ae450, cat_acvEMPChargeFraction
#define CAT_UPGRADE_RamFrigate                      0x00000100  // cat_shipHasRamUpgrade 0x005bf840, RammingFrigateHouseKeep
#define CAT_UPGRADE_DreadnoughtCannon               0x00000400  // DreadnoughtHouseKeep 0x005b0f10
#define CAT_UPGRADE_RamInfectionImmunity            0x00400000  // cat_rammingFrigateCollide 0x005c14a0
#define CAT_UPGRADE_RamCuttingBeam                  0x02000000  // cat_rammingFrigateDamageTarget 0x005c1b20
				

// CAT (M2-R4 combat): "never" for Ship.catSalvageNotifyTime: univCreateShip 0x00542320 and cat_salvageDetach
//      0x004f88b0 store 0x6258d727, the bits of 1e21f
#define CAT_SALVAGE_NEVER                           1e21f

struct Resource;

typedef struct Ship         // Ship object: cataclysm.exe's 0x63C-byte layout (M2-R5 layout pass)
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
    // CAT (M2-R5 layout): every field below is at its cataclysm.exe offset (univCreateShip 0x00542320 initialises
    //      most of them; CET's spaceobj.h asserts 40 of them; the cat* fields carry the function that set or read
    //      them when they were found). HW1's slavelink/slaveinfo, sizeofShipSpecifics and the extra dock vars are
    //      gone. The ILP32 asserts are below the struct.
    sdword catSalvagePlayer;        // +0x238: index of the player whose salvagers reached it first, -1 none
                                    //   (univCreateShip stores -1 at 0x005423f6; Salvage.c)
    Node shiplink;                  // +0x23C
    vector moveTo;                  // +0x24C move destination of the ship
    vector moveFrom;                // +0x258 where the ship is moving from
    vector formationOffset;         // +0x264 offset in formation
    vector formationHeading;        // +0x270 heading in formation (optional)
    vector engineOffset;            // +0x27C engine nozzle offset from position in object coordinates.
    vector enginePosition;          // +0x288 engine position in universe
    struct Ship *rowGetOutOfWay;    // +0x294 Right of way - ship to get out of way of
    vector rowOriginalPoint;        // +0x298 Right of way - my original point
    vector rowOutOfWayPoint;        // +0x2A4 Right of way - point to go to be out of way
    ShipType shiptype;              // +0x2B0
    ShipRace shiprace;              // +0x2B4
    //*******Tactics stuff start
    //FIX LATER to support fewer variables using bitmasks
    TacticsType tacticstype;        // +0x2B8 ships current tactcis setting
    bool32 isDodging;               // +0x2BC flag, true if ship is performing a dodge
    real32 DodgeTime;               // +0x2C0 variable used to control when a ship will stop dodging
    uword  DodgeDir;                // +0x2C4
    uword  pad2;
    bool32 tacticsFormationVar1;    // +0x2C8 variable for formation attacking
    udword DodgeFlag;               // +0x2CC info flag for dodging
    sdword tactics_ordertype;       // +0x2D0 ships order type convulved into the Tactics Domain
    udword tacticsTalk;             // +0x2D4 var used for intership comunication
    sdword tacticsNeedToFlightMan;  // +0x2D8 same as above
    //*******Tactics stuff end
    sdword dockwaitforNorm;         // +0x2DC
    real32 timeCreated;             // +0x2E0
    sdword kamikazeState;           // +0x2E4
    vector kamikazeVector;          // +0x2E8
    real32 speedBurstTime;          // +0x2F4
    real32 singlePlayerSpeedLimiter;// +0x2F8 limiter of ships maxvelocity, single player game only!
    real32 deathtime;               // +0x2FC time at which this ship should blow up, if SOF_Crazy is set
    //mad animation variabes
    udword madAnimationFlags;       // +0x300 general purpose flags for mad animations
    udword madGunStatus;            // +0x304 gun status
    udword madWingStatus;           // +0x308 wing animation status
    udword madDoorStatus;           // +0x30C
    udword madSpecialStatus;        // +0x310
    udword cuedAnimationIndex;      // +0x314 Animation index waiting in the wings to be played
    udword cuedAnimationType;       // +0x318 cue of animation types (guns, wings, etc...)
    udword nextAnim;                // +0x31C
    udword soundEventAnimationTypeFlag;    // +0x320 opening,closing,damaged_opening,damaged_closing
    sdword forceCancelDock;         // +0x324
    real32 gravwellTimeEffect;      // +0x328 time the gravwell effect sould be played again
    //
    struct Player *playerowner;     // +0x32C
    sdword resources;               // +0x330
    real32 fuel;                    // +0x334 (univCreateShip 0x005427c4 still stores maxfuel; nothing burns it)
    real32 lastTimeRepaired;        // +0x338
    real32 magnitudeSquared;        // +0x33C most recent scaling factor
    struct Ship *gettingrocked;     // +0x340 ship which is rocking you (shooting you)
    struct Ship *recentAttacker;    // +0x344 ship which has attacked you recently (ie, not just this AI tick) -- see recentlyAttacked
    struct Ship *firingAtUs;        // +0x348 ship which has recently fired upon us (but needn't have hit) see recentlyFiredUpon
    bool8 shipidle;                 // +0x34C flag to indicate if ship has no command to execute
    bool8 autostabilizeship;        // +0x34D if TRUE, we should stabilize the ship
    ubyte autostabilizestate;       // state of autostabilization
    ubyte autostabilizewait;        // how many frames to wait before stabilizing in autostabilize
    ubyte aistateattack;            // +0x350 AI attack state variable
    ubyte flightmanState1;          // generic state variables for flight maneuvers
    ubyte flightmanState2;          // generic state variables for flight maneuvers
    ubyte rcstate1;                 // state variable for resource collecting
    ubyte rcstate2;                 // +0x354 state variable for resource collecting
    ubyte aistatecommand;
    sbyte dontrotateever;           // +0x356
    sbyte dontapplyforceever;       // +0x357
    bool8 shipisattacking;          // +0x358
    sbyte colorScheme;              // +0x359 what color scheme does this ship use?
    ubyte recentlyAttacked;         // counts down from RECENT_ATTACK_DURATION -- see recentAttacker
    ubyte recentlyFiredUpon;        // same, see firingAtUs
    ubyte chatterBits;              // +0x35C flags for battle chatter
    ubyte rowState;                 // state variable for right of way
    ubyte catPad35E[2];
    real32 aidescend;               // +0x360 AI variable for capital ships descending
    udword flightman;               // +0x364 current flight maneuver handle
    void *flightmanInfo;            // +0x368 pointer to any additional flight maneuver info
    struct CommandToDo *formationcommand;   // +0x36C pointer to formationcommand, if ship in formation
    struct CommandToDo *command;    // +0x370 pointer to ship's current command, if any
    SpecificAttackVars attackvars;  // +0x374 attack information
    struct Effect *rceffect;        // +0x38C effect variable for resource collecting (RemoveShipsFromDoingStuff 0x00442110)
    void (*ShipXHarvestsResourceY)(struct Ship *ship,struct Resource *resource);    // +0x390 function pointer for resource collecting
    sdword visibleToWho;            // +0x394 bitmask flag indicating to which players this ship is visible to...relevant only if ship is cloaked
    sdword visibleToWhoPreviousFrame; // +0x398 same as above, but relevant for the previous univupdate frame
    sdword howDidIDie;              // +0x39C defined above: Set when a ship is set to die, that way, at somepoint (hopefully) we can tell how the bitch died.
    sdword whoKilledMe;             // +0x3A0 playerIndex of the player that killed this ship
    udword nDamageLights;           // +0x3A4
    DamageLightStatic* damageLights;// +0x3A8
    struct Effect* showingDamage[3][3]; // +0x3AC
    vector holdingPatternPos;       // +0x3D0 (processHoldingPattern 0x00446f40)
    real32 damageModifier;          // +0x3DC 1.0 at creation
    real32 catDamageFactor;         // +0x3E0 CAT: 1.0 at creation (0x005426e0); kasfShipsSetDamageFactor 0x00493980 stores
                                    //   damagePercent / 100 here, gunShoot 0x0048b5b0, missileShoot 0x0048aac0 and the
                                    //   Beast weapon scale their damage by it. Name chosen here.
    real32 passiveAttackCancelTimer;// +0x3E4 (clHalt 0x00443a10 stores TW_GLOBAL_PASSIVE_ATTACK_CANCEL_MODIFIER + now)
    real32 catBuildProgress;        // +0x3E8 CAT: external build progress 0..1 (univCreateShip 0x00542320 sets 0 for a
                                    //   ship built externally, else 1.0; cat_clExternalBuildUpdate 0x0044dfc0 advances it)
    struct Ship *catSalvageDepot;   // +0x3EC CAT: the depot it is being delivered to (cat_salvageDeliverToDepot)
    sdword catArmourLevel;          // +0x3F0 CAT: 0..2, the column of cat_damageVsArmourTable a hit on this ship uses
                                    //   (cat_shipUpgradesInit 0x005140a0, cat_shipApplyUpgrades 0x00514490; M2-R4 combat)
    udword catUpgradeFlags;         // +0x3F4 CAT: CAT_UPGRADE_* (cat_shipHasUpgrade 0x00514210; cat_shipUpgradesInit 0x005140a0 clears it)
    //later will make most of these into a single flag
    // CAT: HW1's tractor block is still zeroed by univCreateShip (9 dwords at +0x3F8..+0x418, 0x00542740); nothing in
    //      the exe tractors a ship
    udword tractorbeaminfo;         // +0x3F8
    udword tractorvector;           // +0x3FC
    struct Player *tractorbeam_playerowner; // +0x400
    sdword tractorstate;            // +0x404 info variable for tractorbeaming state
    sdword tractorPosinfo[4];       // +0x408 more tractor info!
    sdword hyperspacePing;          // +0x418 true or false flag indicating if the ship has had a hyperspace ping created for it...NON-deterministic!
                                    //   (processMpHyperspaceingToDo 0x00442e10)
    sdword needNewLeader;           // +0x41C
    struct Ship *newDockWithTransfer;   // +0x420
    sdword newDockIndexTransfer;    // +0x424
    real32 shipDeCloakTime;         // +0x428 time is set when a ship decloaks...this time is used to give decloaking ships an advantage
    shiptrail* trail[MAX_NUM_TRAILS];   // +0x42C ship's engine trail(s)
    ShipID shipID;                  // +0x43C
    uword hotKeyGroup;              // +0x43E what hot-key group this ship belongs to
    sdword retaliateSpeechVar;      // +0x440
    real32 retaliateSpeechTime;     // +0x444
    SOUNDEVENT soundevent;          // +0x448 (soundEventInitStruct at 0x005428b4; hyperspaceHandle +0x45C)
    GunInfo *gunInfo;               // +0x490
    SpecificDockVars dockvars;      // +0x494 dock information (CAT: the exe's five words; see SpecificDockVars)
    DockInfo *dockInfo;             // +0x4A8 (cat_dockInitShipDockInfo 0x00463bb0)
    // CAT (M2-R4 ships-b): the ship that made this one (exe Ship+0x4AC: the 6th argument of univCreateShip 0x00542320,
    //      0 for an ordinary ship; NomadMoonInit, cat_dreadnoughtCreateCannon, cat_univCreateMothershipModules and the
    //      external build pass the maker; the construction manager reads it). Name chosen here.
    struct Ship *catCreator;        // +0x4AC
    struct Ship *catQueuedAttackTarget; // +0x4B0 CAT: cat_univShipDeferredUpdate 0x005447b0 turns it into a clAttack and
                                    //   clears it; cat_beastAttackHit 0x0042af40 sets it for a multiplayer bHeavyCruiser
    ShipsInsideMe *shipsInsideMe;   // +0x4B4
    shipbindings *bindings;         // +0x4B8 ship hierarchy bindings
    madanim *madBindings;           // +0x4BC
    void *catRepairDroids;          // +0x4C0 CAT: the repair-droid array (droidCreateArray 0x005c3070 at univCreateShip
                                    //   0x00542659 for a Somtaaw ship with nRepairDroids; HW1's commented pRepairDroids).
                                    //   The droid runtime is M2-R6 ships' (RepairDroid.h); NULL until then
    struct ShipSinglePlayerGameInfo *shipSinglePlayerGameInfo;     // +0x4C4 stuff for the single player game
    void* lightning[2];             // +0x4C8 lightning effect stuff (HW1; no exe reader of +0x4C8/+0x4CC found)
    // CAT (M2-R4 combat): the Beast infection state (BeastWeapon.c, cat_univBeastInfectionUpdate 0x00543860).
    //      Names chosen here from their users.
    sdword catBeastStunState;       // +0x4D0: 1 hit (stunned), 2 infected (cat_beastAttackHit 0x0042af40)
    real32 catBeastStunStart;       // +0x4D4
    real32 catBeastStunEnd;         // +0x4D8
    real32 catBeastSpin;            // +0x4DC: -1.0 when hit
    sdword catBeastHits;            // +0x4E0: infection hits taken (beastAttacksRequired converts)
    real32 catBeastConvertTime;     // +0x4E4: when the infected ship turns Beast, 0 = not infected
    struct Ship *catBeastInfector;  // +0x4E8
    struct Player *catBeastInfectorOwner;   // +0x4EC
    real32 catBeastCascadeTime;     // +0x4F0: last infection hit
    real32 catBeastCascadePoints;   // +0x4F4: cascade points left to pass on
    real32 catBeastCascadeEnd;      // +0x4F8
    shipbindings *catBeastBindings; // +0x4FC: bindings of the Beast type it turns into
    ubyte catBeastColor;            // +0x500: the infector's colour scheme
    ubyte catPad501[3];
    // CAT (M2-R4 combat): the salvage state of a ship as a salvage target (Salvage.c). univCreateShip 0x00542320
    //      zeroes the block and sets catSalvagePlayer -1, catSalvagePointIndex -1 and catSalvageNotifyTime
    //      CAT_SALVAGE_NEVER (0x6258d727, 1e21). Names chosen here from their users.
    bool8 catSalvageConverting;     // +0x504: drawn as its converted type (cat_salvageDepotProcessGeneric)
    ubyte catPad505[3];
    real32 catSalvageConvertStart;  // +0x508
    real32 catSalvageConvertEnd;    // +0x50C
    shipbindings *catSalvageConvertBindings;    // +0x510: cat_univMeshBindingsDupeNoGuns of the new type
    ubyte catSalvageConvertColor;   // +0x514: the new owner's colour scheme
    ubyte catPad515[3];
    sbyte catRKOMissilesTaken;      // +0x518: missiles a dRKO has gained from eating this ship (RKOHouseKeep
                                    //   0x005c4260, its only user; name chosen here)
    ubyte catPad519[3];
    real32 catLinkHealth[3];        // +0x51C..+0x524: a linked ship's health at the link and its two parents' (cat_processLinkToDo
                                    //      0x00467f98); cat_processUnlinkToDo 0x00468d19 shares the damage out (M2-R5 command-tactics)
    // CAT (M2-R4 ships-b): the helpless / EMP / drift state (cat_empShockShip 0x005b3900, cat_shipSetHelpless
    //      0x005b3bd0, cat_shipClearHelpless 0x005b3c80, cat_shipStartDrift 0x005b3b40, cat_shipUpdateDrift
    //      0x005b3d60). Names chosen here from their users.
    bool8 catEmpStunned;            // +0x528: set by the first EMP shock, cleared with the helplessness;
                                    //   while set the ship's velocity decays (x0.9 per update)
    ubyte catPad529[3];
    sdword catEmpShockCount;        // +0x52C: EMP strength taken so far
    bool32 catHelpless;             // +0x530: set/cleared by cat_shipSetHelpless/ClearHelpless (ShipDisable.c); a helpless
                                    //   crazy ship keeps drifting (physUpdateObjPosVelShip 0x004e4950)
    CatTimer catHelplessTimer;      // +0x534
    bool32 catHelplessSavedDisabled;    // +0x544: SOF_Disabled before the helplessness
    bool32 catHelplessSavedCrazy;       // +0x548: SOF_Crazy before the helplessness
    sdword catHelplessSavedAutostabilize;   // +0x54C: autostabilizeship before the helplessness
    bool32 catDrifting;             // +0x550: set by cat_shipStartDrift, cleared by cat_shipUpdateDrift (ShipDisable.c);
                                    //   read by cat_blastAffectShip 0x00515d30 and physUpdateObjPosVelShip
    vector catDriftVelocity;        // +0x554: the velocity when the drift started
    CatTimer catDriftTimer;         // +0x560: the drift's timer (cat_shipStartDrift, ShipDisable.c)
    // CAT (M2-R4 ships-b): the dissolve state (Dissolve.c, cat_shipDissolveInit 0x00460220 .. cat_shipDissolveStop
    //      0x00460c70). Names chosen here.
    bool32 catDissolving;           // +0x570: set by cat_shipDissolveStart 0x004603d0, cleared by cat_shipDissolveInit 0x00460220
    bool32 catDissolveEffectsOn;    // +0x574: 1 from cat_shipDissolveInit, 0 after cat_shipDissolveStop
    vector catDissolvePoint;        // +0x578: the point the ship dissolves into
    CatTimer catDissolveTimer;      // +0x584
    struct Effect *catDissolveEffect;   // +0x594
    bool32 catUpgrading;            // +0x598: set by cat_clUpgradeShips 0x0044de80, cleared by cat_univShipDeferredUpdate 0x005447b0
    real32 catUpgradeStart;         // +0x59C CAT: cat_clUpgradeShips 0x0044de80 stores the start time (M2-R5 command follow-up)
    real32 catUpgradeEnd;           // +0x5A0 CAT: and the end time, which cat_univShipDeferredUpdate waits for
    vector catDockLatchHeading;     // +0x5A4 CAT: last cone heading of cat_dockFlyToConeOriginLatch 0x00464b30
                                    //   (cat_dockShipDocksAtShip 0x00465860 stores it when the ship is inside the cone)
    udword catShipReserved5B0;      // +0x5B0: univCreateShip 0x00542320 stores 1 (0x00542863); no reader found
    CatTimer catBuildTimer;         // +0x5B4 CAT: the external build timer (CET buildTimer; cat_clWrapStartExternalBuildTimer)
    real32 catProtectedSpeedLimit;  // +0x5C4 CAT: the escort group's speed (cat_univLimitProtectedShipSpeed 0x00544e00,
                                    //   GetShipsTravelVel; M2-R5 command follow-up)
    sdword catKillsByClass[NUM_CLASSES];    // +0x5C8: kills of this ship per class (ApplyDamageToTarget 0x00546250;
                                    //   cat_shipUpdateExperienceLevel 0x005147d0 weighs them)
    sdword catExperienceLevel;      // +0x5F8 CAT: 0..4 (cat_shipUpdateExperienceLevel 0x005147d0; cat_shipGetSpeedMultiplier,
                                    //   cat_shipGetDamageMultiplier, the selection filters)
    real32 catExperienceMessageTime;// +0x5FC CAT: real time of the last "promoted" message (cat_shipUpdateExperienceLevel)
    // CAT (M2-R4 ships-b): the Workers repairing this ship (cat_workerRepairTarget 0x005c89a0, cat_workerChooseRepairTarget
    //      0x005c7d60). Names chosen here.
    sdword catRepairIdleWorkers;    // +0x600: Workers sent to it while it was at full health
    sdword catRepairWorkers;        // +0x604: Workers repairing it
    struct Ship *catSalvageBigGun;  // +0x608: a towed dBigGunUnit's dBigGun, or the salvager's; a Worker also keeps its repair
                                    //   target here (cat_workerRepairTarget 0x005c89a0)
    sdword catKilledByPlayer;       // +0x60C: the player whose weapon killed it (ApplyDamageToTarget, damage 666..668)
    bool32 catBurstFiring;          // +0x610: TRUE in a gun's burst, FALSE in its cool-off (gunCanShoot 0x0048a2f0)
    sbyte catKamikazeInfectionHits; // +0x614: Beast cruise-missile hits taken (cat_kamikazeCheckShip 0x0043a980)
    sbyte catSalvagePointIndex;     // +0x615: the dock point it holds on its target or depot, -1 none
    bool8 catAttackStarted;         // +0x616: set with shipisattacking by gunShoot 0x0048b5b0 (also for the wingman's
                                    //   first shot), cleared by RemoveShipFromAttacking and cat_beastWeaponHouseKeep;
                                    //   read by the blob code (bobBlobItemize, bobUpdateObjsInBlobCollInfo)
    ubyte catPad617;
    real32 catSalvageNotifyTime;    // +0x618: last "more salvagers needed" message (real time)
    real32 catSalvageDepotCheckTime;// +0x61C: last nearest-depot check (cat_salvageSpecialTarget state 10)
    real32 catDockStateTime;        // +0x620 CAT: when a slipgate transit state (0x10/0x11) began (cat_dockShipDocksAtShip)
    CatTimer catLinkTimer;          // +0x624 CAT: the link order's timer (cat_processLinkToDo 0x00467b40; M2-R5 command follow-up)
    NAVLightStaticInfo *catNavLightStatic;  // +0x634 CAT: a copy of the static's navlightStaticInfo (univCreateShip
                                    //   0x005423fc); no reader found. Name chosen here.
    udword ShipSpecifics[1];        // +0x638 this member must go last
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
    // CAT (M2-R5 render): 12 bytes the exe has before the mine fields: missileShoot 0x0048ac6e..0x0048ad27 writes
    //      mineAIState +0x220, formation_position +0x224, FORCE_DROPPED +0x230, formationLink +0x234,
    //      formationinfo +0x244, formation_number_X/Y +0x248/+0x24C and the colour scheme byte +0x250 (also
    //      nisColorScheme 0x004d07b0 and the renderer, cat_rndRenderObjectList 0x00579b08 / the trail pass 0x005770d3).
    //      No reader of +0x214..+0x21F found; name chosen here.
    udword catMissileReserved214[3];            // +0x214
    udword mineAIState;                         //ai state of missile MINES
    vector formation_position;                  //vector that records the position the mine WANTS to be in while in formation
    sdword FORCE_DROPPED;                       //flag..true if force_dropped...used for variable selection
    Node formationLink;
    MineFormationInfo *formationinfo;
    sdword formation_number_X;
    sdword formation_number_Y;
    ubyte colorScheme;              // +0x250 (CAT, M2-R5 render: HW1's +0x244 moved by the 12 bytes above)
    ubyte pad[3];
    // CAT (M2-R5 layout): the exe's Missile is 0x258 bytes (univAddMissile 0x00540c70 allocates 600, LoadSpaceObj
    //      0x00501660 the same). The +0x254 float scales the selection circle when it is nonzero (Select.c, M2-R3)
    //      and the mesh (cat_rndRenderObjectList 0x00579b92, when the static's +0x110 is nonzero). Name chosen here.
    real32 catMissileScale;         // +0x254
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
    // CAT (M2-R4 combat): the exe keeps the firing gun's weapon word here (Bullet +0x108 = Gun +0x00, WEAPON_*;
    //      cat_gunCreateBullet 0x0048bb00), where HW1 keeps the bullet type. Its readers test 2 (WEAPON_ION), which is
    //      HW1's BULLET_Beam: the ion cannon is Cataclysm's beam. 4 (WEAPON_LASER) does not move
    //      (physUpdateBulletPosVel 0x004e45b0), 8 (WEAPON_BEASTINFECT) is steered by the Beast weapon.
    sdword bulletType;                          // CAT: WEAPON_* (was BulletType); HW1's DefenseFighter laser type is gone
                                                //   with the type (M2-R4 ships-b retire)
    bool32 catBurst;                            // CAT: exe +0x10C: fired while the ship burst-fires (specialFlags
                                                //   0x40, SPECIAL_BurstFiring); replaces HW1's BULLET_SpecialBurst type
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
    real32 damage;                              // +0x138 current damage of bullet
    // CAT (M2-R5 layout): HW1's damageFull (+0x13C) is gone: the exe keeps catMoved there (below)
    // CAT (M2-R4 combat): the exe's Bullet is 0x164 bytes (cat_gunCreateBullet 0x0048bb00). Its +0x13C, where HW1 keeps
    //      damageFull, is a flag physUpdateBulletPosVel 0x004e45b0 sets after the first frame: a bullet cannot expire
    //      before it has moved once. M2-R5 layout: at the exe's offsets (the prefix is the exe's; damageFull is gone).
    bool32 catMoved;                            // CAT: exe +0x13C
    udword SpecialEffectFlag;                   // +0x140 Used by special ships to see if they have affected this bullet
    real32 DFGFieldEntryTime;                   // +0x144 used by DFGF.
    real32 BulletSpeed;                         // +0x148
    struct Player *playerowner;                 // +0x14C
    real32 catBigGunRange;                      // CAT: exe +0x150: a GS_BigGun shell is deleted after this distance
    real32 catBigGunTravelled;                  // CAT: exe +0x154: distance a GS_BigGun shell has flown
    vector catPrevPosition;                     // CAT: exe +0x158: where the bullet was before its last move (its
                                                //   start until then; physUpdateBulletPosVel 0x004e45b0)
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
    // HERE UP SAME for Derelicts and Ships
    // CAT (M2-R5 layout): the exe's Derelict is 0x294 bytes (univAddDerelictByStatInfo 0x00541000, LoadSpaceObj
    //      0x00501660), HW1's fields after the shared prefix: derelictlink +0x238, derelictID +0x248 (0x00541077),
    //      colorScheme +0x24A (0x00541306), the sound handles +0x250 (-1 at 0x0054119b), nextRandomTime +0x258,
    //      creationTime +0x28C, derelicttype +0x290
    Node derelictlink;              // +0x238
    DerelictID derelictID;          // +0x248
    sbyte colorScheme;              // +0x24A what color scheme does this ship use?
    ubyte pad[3];
    sdword ambientSoundHandle;      // +0x250 used by sound engine to keep track of the ambient sound the derelict makes
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
    real32 creationTime;            // +0x28C
    DerelictType derelicttype;      // +0x290
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

    AsteroidType asteroidtype;  // +0x198 asteroid specific information
    real32 scaling;             // +0x19C
    // CAT (M2-R5 layout): the exe's Asteroid is 0x1B4 bytes (univAddAsteroid 0x00540a30, LoadSpaceObj 0x00501660):
    //      five dwords follow scaling, of which only +0x1A4 has a known user
    udword catAsteroidReserved1A0;  // +0x1A0
    // CAT (M2-R4 ships-b): the mimic posing as this asteroid (exe Asteroid+0x1A4: MimicHouseKeep 0x005b8f30 sets it on
    //      the fake asteroid of cat_univAddMimicResource 0x005408b0; NULL for a real asteroid). Name chosen here.
    struct Ship *catMimicShip;      // +0x1A4
    udword catAsteroidReserved1A8[3];   // +0x1A8 .. +0x1B0
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
    CAT (M2-R5 layout): the object layouts of cataclysm.exe on ILP32 (tools/m2_layout32.sh). Sources: univCreateShip
    0x00542320 (0x63C-byte ships, the field stores decoded in docs/m2-r5-layout-dock.md), univAddAsteroid 0x00540a30
    (0x1B4), univAddMissile 0x00540c70 (0x258), univAddDerelictByStatInfo 0x00541000 (0x294), LoadSpaceObj 0x00501660
    (bullet 0x164, asteroid 0x1B4, nebula 0x1A8, derelict 0x294, missile 0x258), univAddDustCloud (0x1A4),
    univAddGasCloud (0x1A0) and CET's spaceobj.h ASSERT_OFFSET list for Ship.
=============================================================================*/
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(SpaceObjHeader) == 0x20, "SpaceObjHeader: staticinfo is at +0x20 in cataclysm.exe");
_Static_assert(sizeof(SpaceObj) == 0x98, "SpaceObj: rotinfo is at +0x98 in cataclysm.exe");
CAT_ASSERT_OFFSET(SpaceObj, attributes, 0x38);
CAT_ASSERT_OFFSET(SpaceObj, attributesParam, 0x3C);
CAT_ASSERT_OFFSET(SpaceObj, cameraDistanceSquared, 0x40);
CAT_ASSERT_OFFSET(SpaceObj, collMyBlob, 0x54);
CAT_ASSERT_OFFSET(SpaceObj, posinfo, 0x58);
CAT_ASSERT_OFFSET(SpaceObj, catMeshState, 0x80);
CAT_ASSERT_OFFSET(SpaceObjRot, rotinfo, 0x98);
CAT_ASSERT_OFFSET(SpaceObjRotImp, collInfo, 0xD4);
CAT_ASSERT_OFFSET(SpaceObjRotImp, collInfo.precise, 0x14C);
CAT_ASSERT_OFFSET(SpaceObjRotImp, impactablelink, 0x160);
CAT_ASSERT_OFFSET(SpaceObjRotImpTarg, health, 0x170);
CAT_ASSERT_OFFSET(SpaceObjRotImpTargGuidance, nonstatvars, 0x17C);
CAT_ASSERT_OFFSET(SpaceObjRotImpTargGuidance, nonstatvars.catMaxVelocity, 0x1B8);
CAT_ASSERT_OFFSET(SpaceObjRotImpTargGuidance, clampInfo, 0x1BC);
_Static_assert(sizeof(ShipOrDerelict) == 0x238, "ShipOrDerelict: the shared ship/derelict prefix ends at +0x238");
CAT_ASSERT_OFFSET(ShipOrDerelict, aistate, 0x1C0);
CAT_ASSERT_OFFSET(ShipOrDerelict, putOnDoor, 0x1C3);
CAT_ASSERT_OFFSET(ShipOrDerelict, dockingship, 0x1C4);
CAT_ASSERT_OFFSET(ShipOrDerelict, specialFlags, 0x1C8);
CAT_ASSERT_OFFSET(ShipOrDerelict, specialFlags2, 0x1CC);
CAT_ASSERT_OFFSET(ShipOrDerelict, catRetreating, 0x1D0);
CAT_ASSERT_OFFSET(ShipOrDerelict, catResourceFinished, 0x1D4);
CAT_ASSERT_OFFSET(ShipOrDerelict, catDisableReason, 0x1DC);
CAT_ASSERT_OFFSET(ShipOrDerelict, catDisabled, 0x210);
CAT_ASSERT_OFFSET(ShipOrDerelict, salvageNumTagged, 0x214);
CAT_ASSERT_OFFSET(ShipOrDerelict, navLightInfo, 0x234);
// the Ship (CET's Ship offsets, all but three where the exe contradicts CET: tractorPosinfo 0x408 not 0x400, nothing at
//      0x650 / 0x654 past the 0x63C ship, health not buildProgress at 0x170; and the cat* fields' sources) [audit: M2-R5
//      layout-dock: was "CET's 40 offsets"; CET has 66]
CAT_ASSERT_OFFSET(Ship, catSalvagePlayer, 0x238);
CAT_ASSERT_OFFSET(Ship, shiplink, 0x23C);
CAT_ASSERT_OFFSET(Ship, enginePosition, 0x288);
CAT_ASSERT_OFFSET(Ship, shiptype, 0x2B0);
CAT_ASSERT_OFFSET(Ship, shiprace, 0x2B4);
CAT_ASSERT_OFFSET(Ship, tacticstype, 0x2B8);
CAT_ASSERT_OFFSET(Ship, isDodging, 0x2BC);
CAT_ASSERT_OFFSET(Ship, DodgeTime, 0x2C0);
CAT_ASSERT_OFFSET(Ship, tacticsFormationVar1, 0x2C8);
CAT_ASSERT_OFFSET(Ship, tactics_ordertype, 0x2D0);
CAT_ASSERT_OFFSET(Ship, tacticsTalk, 0x2D4);
CAT_ASSERT_OFFSET(Ship, tacticsNeedToFlightMan, 0x2D8);
CAT_ASSERT_OFFSET(Ship, dockwaitforNorm, 0x2DC);
CAT_ASSERT_OFFSET(Ship, timeCreated, 0x2E0);
CAT_ASSERT_OFFSET(Ship, madAnimationFlags, 0x300);
CAT_ASSERT_OFFSET(Ship, madGunStatus, 0x304);
CAT_ASSERT_OFFSET(Ship, madWingStatus, 0x308);
CAT_ASSERT_OFFSET(Ship, madDoorStatus, 0x30C);
CAT_ASSERT_OFFSET(Ship, madSpecialStatus, 0x310);
CAT_ASSERT_OFFSET(Ship, cuedAnimationIndex, 0x314);
CAT_ASSERT_OFFSET(Ship, cuedAnimationType, 0x318);
CAT_ASSERT_OFFSET(Ship, playerowner, 0x32C);
CAT_ASSERT_OFFSET(Ship, resources, 0x330);
CAT_ASSERT_OFFSET(Ship, fuel, 0x334);
CAT_ASSERT_OFFSET(Ship, gettingrocked, 0x340);
CAT_ASSERT_OFFSET(Ship, shipidle, 0x34C);
CAT_ASSERT_OFFSET(Ship, autostabilizeship, 0x34D);
CAT_ASSERT_OFFSET(Ship, dontrotateever, 0x356);
CAT_ASSERT_OFFSET(Ship, colorScheme, 0x359);
CAT_ASSERT_OFFSET(Ship, command, 0x370);
CAT_ASSERT_OFFSET(Ship, attackvars.multipleAttackTargets, 0x374);
CAT_ASSERT_OFFSET(Ship, attackvars.attacktarget, 0x378);
CAT_ASSERT_OFFSET(Ship, attackvars.myLeaderIs, 0x37C);
CAT_ASSERT_OFFSET(Ship, attackvars.myWingmanIs, 0x380);
CAT_ASSERT_OFFSET(Ship, attackvars.flightmansLeft, 0x384);
CAT_ASSERT_OFFSET(Ship, attackvars.attacksituation, 0x386);
CAT_ASSERT_OFFSET(Ship, attackvars.attackevents, 0x388);
CAT_ASSERT_OFFSET(Ship, rceffect, 0x38C);
CAT_ASSERT_OFFSET(Ship, visibleToWho, 0x394);
CAT_ASSERT_OFFSET(Ship, howDidIDie, 0x39C);
CAT_ASSERT_OFFSET(Ship, nDamageLights, 0x3A4);
CAT_ASSERT_OFFSET(Ship, damageLights, 0x3A8);
CAT_ASSERT_OFFSET(Ship, showingDamage, 0x3AC);
CAT_ASSERT_OFFSET(Ship, holdingPatternPos, 0x3D0);
CAT_ASSERT_OFFSET(Ship, damageModifier, 0x3DC);
CAT_ASSERT_OFFSET(Ship, catDamageFactor, 0x3E0);
CAT_ASSERT_OFFSET(Ship, passiveAttackCancelTimer, 0x3E4);
CAT_ASSERT_OFFSET(Ship, catBuildProgress, 0x3E8);
CAT_ASSERT_OFFSET(Ship, catSalvageDepot, 0x3EC);
CAT_ASSERT_OFFSET(Ship, catArmourLevel, 0x3F0);
CAT_ASSERT_OFFSET(Ship, catUpgradeFlags, 0x3F4);
CAT_ASSERT_OFFSET(Ship, tractorbeaminfo, 0x3F8);
CAT_ASSERT_OFFSET(Ship, hyperspacePing, 0x418);
CAT_ASSERT_OFFSET(Ship, needNewLeader, 0x41C);
CAT_ASSERT_OFFSET(Ship, shipDeCloakTime, 0x428);
CAT_ASSERT_OFFSET(Ship, trail, 0x42C);
CAT_ASSERT_OFFSET(Ship, shipID, 0x43C);
CAT_ASSERT_OFFSET(Ship, hotKeyGroup, 0x43E);
CAT_ASSERT_OFFSET(Ship, soundevent, 0x448);
CAT_ASSERT_OFFSET(Ship, soundevent.hyperspaceHandle, 0x45C);
CAT_ASSERT_OFFSET(Ship, gunInfo, 0x490);
CAT_ASSERT_OFFSET(Ship, dockvars.dockship, 0x494);
CAT_ASSERT_OFFSET(Ship, dockvars.catDockDistance, 0x498);
CAT_ASSERT_OFFSET(Ship, dockvars.dockstate, 0x49C);
CAT_ASSERT_OFFSET(Ship, dockvars.catDockPointIndex, 0x4A0);
CAT_ASSERT_OFFSET(Ship, dockvars.catDockTime, 0x4A4);
CAT_ASSERT_OFFSET(Ship, dockInfo, 0x4A8);
CAT_ASSERT_OFFSET(Ship, catCreator, 0x4AC);
CAT_ASSERT_OFFSET(Ship, shipsInsideMe, 0x4B4);
CAT_ASSERT_OFFSET(Ship, bindings, 0x4B8);
CAT_ASSERT_OFFSET(Ship, madBindings, 0x4BC);
CAT_ASSERT_OFFSET(Ship, catRepairDroids, 0x4C0);
CAT_ASSERT_OFFSET(Ship, shipSinglePlayerGameInfo, 0x4C4);
CAT_ASSERT_OFFSET(Ship, catBeastStunState, 0x4D0);
CAT_ASSERT_OFFSET(Ship, catBeastBindings, 0x4FC);
CAT_ASSERT_OFFSET(Ship, catSalvageConverting, 0x504);
CAT_ASSERT_OFFSET(Ship, catSalvageConvertColor, 0x514);
CAT_ASSERT_OFFSET(Ship, catRKOMissilesTaken, 0x518);
CAT_ASSERT_OFFSET(Ship, catEmpStunned, 0x528);
CAT_ASSERT_OFFSET(Ship, catHelpless, 0x530);
CAT_ASSERT_OFFSET(Ship, catHelplessTimer, 0x534);
CAT_ASSERT_OFFSET(Ship, catDrifting, 0x550);
CAT_ASSERT_OFFSET(Ship, catDriftTimer, 0x560);
CAT_ASSERT_OFFSET(Ship, catDissolving, 0x570);
CAT_ASSERT_OFFSET(Ship, catDissolveEffect, 0x594);
CAT_ASSERT_OFFSET(Ship, catUpgrading, 0x598);
CAT_ASSERT_OFFSET(Ship, catUpgradeEnd, 0x5A0);
CAT_ASSERT_OFFSET(Ship, catDockLatchHeading, 0x5A4);
CAT_ASSERT_OFFSET(Ship, catBuildTimer, 0x5B4);
CAT_ASSERT_OFFSET(Ship, catProtectedSpeedLimit, 0x5C4);
CAT_ASSERT_OFFSET(Ship, catKillsByClass, 0x5C8);
CAT_ASSERT_OFFSET(Ship, catExperienceLevel, 0x5F8);
CAT_ASSERT_OFFSET(Ship, catRepairIdleWorkers, 0x600);
CAT_ASSERT_OFFSET(Ship, catSalvageBigGun, 0x608);
CAT_ASSERT_OFFSET(Ship, catKilledByPlayer, 0x60C);
CAT_ASSERT_OFFSET(Ship, catBurstFiring, 0x610);
CAT_ASSERT_OFFSET(Ship, catKamikazeInfectionHits, 0x614);
CAT_ASSERT_OFFSET(Ship, catSalvagePointIndex, 0x615);
CAT_ASSERT_OFFSET(Ship, catAttackStarted, 0x616);
CAT_ASSERT_OFFSET(Ship, catSalvageNotifyTime, 0x618);
CAT_ASSERT_OFFSET(Ship, catDockStateTime, 0x620);
CAT_ASSERT_OFFSET(Ship, catLinkTimer, 0x624);
CAT_ASSERT_OFFSET(Ship, catNavLightStatic, 0x634);
CAT_ASSERT_OFFSET(Ship, ShipSpecifics, 0x638);
_Static_assert(sizeof(Ship) == 0x63C, "Ship is 0x63C bytes in cataclysm.exe (univCreateShip 0x00542320)");
// the Missile (univAddMissile 0x00540c70: the HW1 fields in HW1's order after the exe's prefix)
_Static_assert(sizeof(Missile) == 0x258, "Missile is 0x258 bytes in cataclysm.exe (univAddMissile allocates 600)");
CAT_ASSERT_OFFSET(Missile, trail, 0x1C0);
CAT_ASSERT_OFFSET(Missile, missilelink, 0x1D0);
CAT_ASSERT_OFFSET(Missile, missileType, 0x1E0);
CAT_ASSERT_OFFSET(Missile, playerowner, 0x1F8);
CAT_ASSERT_OFFSET(Missile, missileID, 0x210);
CAT_ASSERT_OFFSET(Missile, mineAIState, 0x220);
CAT_ASSERT_OFFSET(Missile, formationLink, 0x234);
CAT_ASSERT_OFFSET(Missile, formationinfo, 0x244);
CAT_ASSERT_OFFSET(Missile, colorScheme, 0x250);
CAT_ASSERT_OFFSET(Missile, catMissileScale, 0x254);
// the Bullet (cat_gunCreateBullet 0x0048bb00, CET's two asserts)
_Static_assert(sizeof(Bullet) == 0x164, "Bullet is 0x164 bytes in cataclysm.exe");
CAT_ASSERT_OFFSET(Bullet, bulletlink, 0xD4);
CAT_ASSERT_OFFSET(Bullet, bulletType, 0x108);
CAT_ASSERT_OFFSET(Bullet, owner, 0x11C);
CAT_ASSERT_OFFSET(Bullet, catMoved, 0x13C);
CAT_ASSERT_OFFSET(Bullet, playerowner, 0x14C);
CAT_ASSERT_OFFSET(Bullet, catBigGunRange, 0x150);
CAT_ASSERT_OFFSET(Bullet, catPrevPosition, 0x158);
// the Derelict and the resources
_Static_assert(sizeof(Derelict) == 0x294, "Derelict is 0x294 bytes in cataclysm.exe");
CAT_ASSERT_OFFSET(Derelict, derelictlink, 0x238);
CAT_ASSERT_OFFSET(Derelict, derelictID, 0x248);
CAT_ASSERT_OFFSET(Derelict, colorScheme, 0x24A);
CAT_ASSERT_OFFSET(Derelict, ambientSoundHandle, 0x250);
CAT_ASSERT_OFFSET(Derelict, nextRandomTime, 0x258);
CAT_ASSERT_OFFSET(Derelict, needNewLeader, 0x280);
CAT_ASSERT_OFFSET(Derelict, creationTime, 0x28C);
CAT_ASSERT_OFFSET(Derelict, derelicttype, 0x290);
_Static_assert(sizeof(Resource) == 0x198, "Resource: the resource prefix ends at +0x198 in cataclysm.exe");
CAT_ASSERT_OFFSET(Resource, resourcelink, 0x17C);
CAT_ASSERT_OFFSET(Resource, resourceID, 0x18C);
CAT_ASSERT_OFFSET(Resource, resourcevalue, 0x190);
CAT_ASSERT_OFFSET(Resource, resourceVolume, 0x194);
_Static_assert(sizeof(Asteroid) == 0x1B4, "Asteroid is 0x1B4 bytes in cataclysm.exe");
CAT_ASSERT_OFFSET(Asteroid, asteroidtype, 0x198);
CAT_ASSERT_OFFSET(Asteroid, catMimicShip, 0x1A4);
_Static_assert(sizeof(Nebula) == 0x1A8, "Nebula is 0x1A8 bytes in cataclysm.exe");
_Static_assert(sizeof(GasCloud) == 0x1A0, "GasCloud is 0x1A0 bytes in cataclysm.exe");
_Static_assert(sizeof(DustCloud) == 0x1A4, "DustCloud is 0x1A4 bytes in cataclysm.exe");
#endif

/*=============================================================================
    Macros:
=============================================================================*/

#define sizeofGunInfo(n) (sizeof(GunInfo) + (n-1)*sizeof(Gun))
#define sizeofGunStaticInfo(n) (sizeof(GunStaticInfo) + (n-1)*sizeof(GunStatic))
#define sizeofDockStaticInfo(n) (sizeof(DockStaticInfo) + (n-1)*sizeof(DockStaticPoint))
#define sizeofNavLightStaticInfo(n) (sizeof(NAVLightStaticInfo) + (n-1)*sizeof(NAVLightStatic))
#define sizeofDockInfo(n) (sizeof(DockInfo) + ((n)-1)*sizeof(sdword))   // CAT: 0x24 + 4n (cat_dockInitShipDockInfo 0x00463bb0)
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

