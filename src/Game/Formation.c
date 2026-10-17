/*=============================================================================
    Name    : Formation.c
    Purpose : Implementation of formations

    Created 10/20/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "Formation.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AIShip.h"
#include "AITrack.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "Debug.h"
#include "FastMath.h"
#include "Globals.h"
#include "Memory.h"
#include "ProximitySensor.h"
#include "Select.h"
#include "Sentinel.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "StatScript.h"
#include "StringSupport.h"
#include "Tactics.h"
#include "Tweak.h"
#include "Universe.h"
#include "UnivUpdate.h"

//real32 DELTA_FORMATION_PADDING = 50.0f;
//real32 BROAD_FORMATION_PADDING = 50.0f;
real32 DELTA_FORMATION_PADDING_SCALE = 1.5f;
real32 BROAD_FORMATION_PADDING_SCALE = 1.5f;
real32 CLAW_FORMATION_PADDING_SCALE = 1.5f;
real32 DELTA3D_FORMATION_PADDING_SCALE = 1.5f;
real32 WALL_FORMATION_PADDING_SCALE = 2.0f;
real32 WALL_FORMATION_PADDING = 20.0f;
real32 SPHERE_FORMATION_PADDING_SCALE = 4.0f;
real32 CLAW_FORMATION_CURVATURE = 1.1f;
real32 SPHERE_MINIMUM_RADIUS = 500.0f;
real32 SPHERE_MAXIMUM_RADIUS = 3500.0f;
real32 SPHERESIZE_IGNORE_NUMDECLINATIONS = 600.0f;

real32 STRIKECRAFT_PADDING_MODIFIER = 1.2f;

real32 FORMATION_TRAVELVEL_MAXSCALE = 1.0f;
real32 FORMATION_TRAVELVEL_MINSCALE = 0.7f;
real32 FORMATION_ERROR_BIG = 10000.0f;
real32 FORMATION_ERROR_NEGLIGIBLE = 100.0f;

real32 PARADE_CONSIDERED_STEADY_VELSQR = 2500.0f;

bool8 FORMATION_TRAVEL_SLOWEST_SHIP = TRUE;

real32 TARGETDRONE_FORMATION_SIZE = 1000.0f;

/*-----------------------------------------------------------------------------
    CAT (M2-R4 command, M2-R5 command-tactics): state of this file against cataclysm.exe. Ported: the formation name
    table (types 9/10), the parade code (GetShipSlot, PutShipInMilitaryParade, CreateMilitaryGroupAroundShip, the
    parade scripts, processMilitaryParadeToDo, setMilitaryParade, paradeSetTweakables) and, in M2-R5, the
    force-field formations: FormationCommand's force-field level (+0x288, FillInFormationSpecifics' fourth argument),
    clFormation 0x00484c80 with 4 arguments, FormationCalculateOffsets (SetFormationOffsetsForForceFieldWall, the
    Sentinel sphere), formationSpecificsSphere, createSphereStaticInfo, setFormationToDo, processFormationToDo and
    GetShipsTravelVel (each marked CAT at the change; docs/m2-r5-command-tactics.md "formation").
-----------------------------------------------------------------------------*/
static NumStrXlate formationtypeinfo[] =
{
    { (uword)DELTA_FORMATION,  strdollar(DELTA_FORMATION) },
    { (uword)BROAD_FORMATION, strdollar(BROAD_FORMATION) },
    { (uword)DELTA3D_FORMATION, strdollar(DELTA3D_FORMATION) },
    { (uword)CLAW_FORMATION, strdollar(CLAW_FORMATION) },
    { (uword)WALL_FORMATION, strdollar(WALL_FORMATION) },
    { (uword)SPHERE_FORMATION, strdollar(SPHERE_FORMATION) },
    { (uword)CUSTOM_FORMATION, strdollar(CUSTOM_FORMATION) },
    { (uword)NO_FORMATION, strdollar(NO_FORMATION) },
    { (uword)SAME_FORMATION, strdollar(SAME_FORMATION) },
    // CAT: the exe's table (0x0087f998, StrToTypeOfFormation 0x00481190) has the two force-field formations too
    { (uword)FORCEFIELD_WALL_FORMATION, strdollar(FORCEFIELD_WALL_FORMATION) },
    { (uword)FORCEFIELD_SPHERE_FORMATION, strdollar(FORCEFIELD_SPHERE_FORMATION) },
    { 0,NULL }
};

static NumStrXlate formationtypeniceinfo[] =
{
    { (uword)DELTA_FORMATION,  "Delta Wing" },
    { (uword)BROAD_FORMATION, "Broad Attack" },
    { (uword)DELTA3D_FORMATION, "Formation X" },
    { (uword)CLAW_FORMATION, "The Claw" },
    { (uword)WALL_FORMATION, "The Wall" },
    { (uword)SPHERE_FORMATION, "Sphere" },
    { (uword)CUSTOM_FORMATION, "Custom" },
    { 0,NULL }
};

scriptEntry FormationTweaks[] =
{
    makeEntry(FORMATION_TRAVEL_SLOWEST_SHIP,scriptSetBool8),
    makeEntry(DELTA_FORMATION_PADDING_SCALE,scriptSetReal32CB),
    makeEntry(BROAD_FORMATION_PADDING_SCALE,scriptSetReal32CB),
    makeEntry(DELTA3D_FORMATION_PADDING_SCALE,scriptSetReal32CB),
    makeEntry(CLAW_FORMATION_PADDING_SCALE,scriptSetReal32CB),
    makeEntry(CLAW_FORMATION_CURVATURE,scriptSetReal32CB),
    makeEntry(WALL_FORMATION_PADDING_SCALE,scriptSetReal32CB),
    makeEntry(WALL_FORMATION_PADDING,scriptSetReal32CB),
    makeEntry(SPHERE_FORMATION_PADDING_SCALE,scriptSetReal32CB),
    makeEntry(SPHERE_MINIMUM_RADIUS,scriptSetReal32CB),
    makeEntry(SPHERE_MAXIMUM_RADIUS,scriptSetReal32CB),
    makeEntry(STRIKECRAFT_PADDING_MODIFIER,scriptSetReal32CB),
    makeEntry(FORMATION_ERROR_CALCULATE_RATE,scriptSetSdwordCB),
    makeEntry(FORMATION_TRAVELVEL_MAXSCALE,scriptSetReal32CB),
    makeEntry(FORMATION_TRAVELVEL_MINSCALE,scriptSetReal32CB),
    makeEntry(FORMATION_ERROR_BIG,scriptSetReal32CB),
    makeEntry(FORMATION_ERROR_NEGLIGIBLE,scriptSetReal32CB),
    makeEntry(TARGETDRONE_FORMATION_SIZE,scriptSetReal32CB),
    makeEntry(SPHERESIZE_IGNORE_NUMDECLINATIONS,scriptSetReal32CB),

    END_SCRIPT_ENTRY
};

// CAT: 11 entries in the exe (0x0087fb38, all FORMATION_SORT_BIGGEST_THEN_CLOSEST): clFormation 0x00484c80 and
//      formationArrageCrazyOptimum index it with the force-field formations (9, 10) too
udword formationSortType[FORCEFIELD_SPHERE_FORMATION + 1] =
{
    FORMATION_SORT_BIGGEST_THEN_CLOSEST,      //DELTA_FORMATION
    FORMATION_SORT_BIGGEST_THEN_CLOSEST,      //BROAD_FORMATION
    FORMATION_SORT_BIGGEST_THEN_CLOSEST,      //DELTA3D_FORMATION
    FORMATION_SORT_BIGGEST_THEN_CLOSEST,      //CLAW_FORMATION
    FORMATION_SORT_BIGGEST_THEN_CLOSEST,      //WALL_FORMATION
    FORMATION_SORT_BIGGEST_THEN_CLOSEST,      //SPHERE_FORMATION
    FORMATION_SORT_BIGGEST_THEN_CLOSEST,      //CUSTOM_FORMATION
    FORMATION_SORT_BIGGEST_THEN_CLOSEST,      //NO_FORMATION (never sorted)
    FORMATION_SORT_BIGGEST_THEN_CLOSEST,      //SAME_FORMATION
    FORMATION_SORT_BIGGEST_THEN_CLOSEST,      //FORCEFIELD_WALL_FORMATION
    FORMATION_SORT_BIGGEST_THEN_CLOSEST,      //FORCEFIELD_SPHERE_FORMATION
};

//prototypes!
void formationArrageCrazyOptimum(CommandToDo *formationcommand);

bool32 isCapitalShipStaticOrBig(ShipStaticInfo *shipstatic)
{
    if (isCapitalShipStatic(shipstatic))
    {
        return TRUE;
    }

    // CAT: both races' cloak and grav-well generators (isCapitalShipStaticOrBig 0x00481130: types 0x3f, 0x40, 0x4d, 0x4f)
    switch (shipstatic->shiptype)   // other cases that are not necessarily covered by isCapitalShipStatic
    {
        case kCloakGenerator:
        case kGravWellGenerator:
        case tCloakGenerator:
        case tGravWellGenerator:
            return TRUE;
        default:
            return FALSE;
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : TypeOfFormationToStr
    Description : converts TypeOfFormation to its string equivalent
    Inputs      : formation
    Outputs     :
    Return      : string representing formation
----------------------------------------------------------------------------*/
char *TypeOfFormationToStr(TypeOfFormation formation)
{
    return NumToStr(formationtypeinfo,(uword)formation);
}

/*-----------------------------------------------------------------------------
    Name        : StrToTypeOfFormation
    Description : converts from string to TypeOfFormation
    Inputs      : str
    Outputs     :
    Return      : returns TypeOfFormation enumeration based on string
----------------------------------------------------------------------------*/
TypeOfFormation NiceStrToTypeOfFormation(char *str)
{
    return StrToNum(formationtypeniceinfo,str);
}

/*-----------------------------------------------------------------------------
    Name        : TypeOfFormationToStr
    Description : converts TypeOfFormation to its string equivalent
    Inputs      : formation
    Outputs     :
    Return      : string representing formation
----------------------------------------------------------------------------*/
char *TypeOfFormationToNiceStr(TypeOfFormation formation)
{
    if (formation == PARADE_FORMATION)
        return strGetString(strPARADE_FORMATION);
    else
        return (strGetString(formation+strFormationOffset));
}

/*-----------------------------------------------------------------------------
    Name        : StrToTypeOfFormation
    Description : converts from string to TypeOfFormation
    Inputs      : str
    Outputs     :
    Return      : returns TypeOfFormation enumeration based on string
----------------------------------------------------------------------------*/
TypeOfFormation StrToTypeOfFormation(char *str)
{
    return StrToNum(formationtypeinfo,str);
}

typedef struct
{
    real32 distance;
    ShipPtr ShipPtr;
} ShipDistInfo;

int compareSizeThenDist(const void *arg1,const void *arg2)
{
    ShipDistInfo *a1 = (ShipDistInfo *)arg1;
    ShipDistInfo *a2 = (ShipDistInfo *)arg2;
    real32 size1 = a1->ShipPtr->staticinfo->common.staticCollInfo.collspheresize;
    real32 size2 = a2->ShipPtr->staticinfo->common.staticCollInfo.collspheresize;

    if (size1 == size2)
    {
        if (a1->distance < a2->distance)
        {
            return -1;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        if (size1 > size2)
        {
            return -1;
        }
        else
        {
            return 1;
        }
    }
}

void formationBiggestThenClosestOptimum(SelectCommand *selection,TypeOfFormation formationtype)
{
    sdword numShips = selection->numShips;
    vector avgpos = { 0.0f,0.0f,0.0f };

    real32 contender;
    real32 biggestship = 0.0f;
    real32 smallestship = 1000000.0f;
    sdword biggestshipindex = -1;
    real32 temp;

    ShipDistInfo *shipdist;
    Ship *tempship;
    sdword i = 0, cloakinsel = 0, cloakindex = 0;
    vector diff;

    for (i=0;i<numShips;i++)
    {
        contender = (selection->ShipPtr[i])->staticinfo->common.staticCollInfo.collspheresize;
        if (contender > biggestship)
        {
            biggestship = contender;
            biggestshipindex = i;
        }
        if (contender < smallestship)
        {
            smallestship = contender;
        }
    }

    dbgAssertOrIgnore(biggestshipindex >= 0);

    if (smallestship == biggestship)        // all ships are the same size
    {
        for (i=0;i<numShips;i++)            // so find average position of all ships
        {
            vecAddTo(avgpos,(selection->ShipPtr[i])->obj.posinfo.position);
        }
        vecDivideByScalar(avgpos,(real32)numShips,temp);
    }
    else                                    // ships are not same size
    {
        // so let average position be position of biggest ship
        avgpos = (selection->ShipPtr[biggestshipindex])->obj.posinfo.position;
    }

    shipdist = memAlloc(sizeof(ShipDistInfo)*selection->numShips,"TmpShipDistArray",0);

    for (i=0;i<selection->numShips;i++)
    {
        vecSub(diff,(selection->ShipPtr[i])->obj.posinfo.position,avgpos);
        shipdist[i].distance = vecMagnitudeSquared(diff);
        shipdist[i].ShipPtr = selection->ShipPtr[i];
    }

    qsort(shipdist,selection->numShips,sizeof(ShipDistInfo),compareSizeThenDist);

    for (i=0;i<selection->numShips;i++)
    {
        selection->ShipPtr[i] = shipdist[i].ShipPtr;
    }

    cloakinsel = FALSE;
    for (i=0;i<selection->numShips;i++)
    {
        // CAT: both races' cloak generators (formationArrangeOptimum 0x00481200 tests 0x3f and 0x4d)
        if((selection->ShipPtr[i]->shiptype == kCloakGenerator) || (selection->ShipPtr[i]->shiptype == tCloakGenerator))
        {
            cloakinsel = TRUE;
            cloakindex = i;
        }
    }

    if(cloakinsel)
    {
        //go through selection, and move any cloak generator to the
        //leader position, that way we cover other ships with cloaking
        //field more effectivly
        tempship = selection->ShipPtr[0];
        selection->ShipPtr[0] = selection->ShipPtr[cloakindex];
        selection->ShipPtr[cloakindex] = tempship;
    }

    memFree(shipdist);

}

void formationArrangeOptimum(struct CommandToDo *formationtodo)
{
    SelectCommand *selection = formationtodo->selection;
    sdword numShips = selection->numShips;

    dbgAssertOrIgnore(numShips >= ABSOLUTE_MIN_SHIPS_IN_FORMATION);

    if (numShips == 1)
    {
        return;     // only 1 item, so don't sort
    }

    switch (formationSortType[formationtodo->formation.formationtype])
    {
        case FORMATION_SORT_BIGGEST_THEN_CLOSEST:
            formationBiggestThenClosestOptimum(selection,formationtodo->formation.formationtype);
            formationtodo->formation.sortorder = FORMATION_SORT_BIGGEST_THEN_CLOSEST;
            break;

        default:
            dbgAssertOrIgnore(FALSE);
            break;
    }
}

#define NUM_SPHERE_TABLE_ENTRYS     50

// CAT: a force-field sphere holds 12 Sentinels (clFormation 0x00484c80 caps it at 12) plus the protected ship: the
//      sphere table entry for 13 ships (createSphereStaticInfo 0x00481410)
#define MAX_SENTINELS_IN_FORCE_FIELD_SPHERE     12
static SphereTableEntry *catForceFieldSphereEntry = NULL;

struct SphereStaticInfo *createSphereStaticInfo(void)
{
    sdword sizeofspherestaticinfo;
    SphereStaticInfo *sphereStaticInfo;
    sdword tableindex;
    SphereTableEntry *tableentry;
    sdword sizeofSphereTableEntry;
    sdword numDeclinations;
    sdword numCircleDivisions;
    real32 angleDivision;
    real32 angle;
    real32 rotang;
    sdword totalships;
    sdword numships;
    sdword i;
    SphereDeclination *sphereDeclination;

    sizeofspherestaticinfo = sizeofSphereStaticInfo(NUM_SPHERE_TABLE_ENTRYS);

    sphereStaticInfo = memAlloc(sizeofspherestaticinfo,"spherestatinfo",NonVolatile);
    memset(sphereStaticInfo,0,sizeofspherestaticinfo);

    sphereStaticInfo->numTableEntries = NUM_SPHERE_TABLE_ENTRYS;

    for (tableindex=0;tableindex<NUM_SPHERE_TABLE_ENTRYS;tableindex++)
    {
        numDeclinations = tableindex+1;
        numCircleDivisions = numDeclinations + numDeclinations + 2;
        angleDivision = TWOPI / ((real32)numCircleDivisions);

        sizeofSphereTableEntry = sizeofSphereTableEntry(numDeclinations);
        tableentry = memAlloc(sizeofSphereTableEntry,"spheretabentry",NonVolatile);
        memset(tableentry,0,sizeofSphereTableEntry);

        sphereStaticInfo->sphereTableEntryPtrs[tableindex] = tableentry;

        totalships = 0;
        tableentry->numDeclinations = numDeclinations;
        angle = PI / 2.0f;     // 90 degrees

        for (i=0,sphereDeclination=&tableentry->sphereDeclinations[0];i<numDeclinations;i++,sphereDeclination++)
        {
            angle -= angleDivision;
            numships = (sdword) (cos(angle) * (real32)numCircleDivisions);
            totalships += numships;

            sphereDeclination->cosdeclination = (real32)cos(angle);
            sphereDeclination->sindeclination = (real32)sin(angle);
            sphereDeclination->numAnglePoints = numships;
            rotang = TWOPI / (real32)numships;
            sphereDeclination->cosrotang = (real32)cos(rotang);
            sphereDeclination->sinrotang = (real32)sin(rotang);
        }

        tableentry->numShipsCanHandle = totalships + 3;   // leader goes in center, two ships at poles (+3)

    }

    // CAT: the table entry of exactly MAX_SENTINELS_IN_FORCE_FIELD_SPHERE ships is the force-field sphere's
    //      (createSphereStaticInfo 0x00481542: the last entry for 13 ships, kept at 0x00a436f8)
    catForceFieldSphereEntry = NULL;
    for (tableindex=0;tableindex<NUM_SPHERE_TABLE_ENTRYS;tableindex++)
    {
        if (sphereStaticInfo->sphereTableEntryPtrs[tableindex]->numShipsCanHandle == MAX_SENTINELS_IN_FORCE_FIELD_SPHERE + 1)
        {
            catForceFieldSphereEntry = sphereStaticInfo->sphereTableEntryPtrs[tableindex];
        }
    }
    if (catForceFieldSphereEntry == NULL)
    {
        dbgFatal(DBG_Loc,"MAX_SENTINELS_IN_FORCE_FIELD_SPHERE is not set to a valid value for handling Spherical Force Fields.");
    }

    return sphereStaticInfo;
}

/*-----------------------------------------------------------------------------
    Name        : freeSphereStaticInfo
    Description : frees sphereStaticInfo
    Inputs      : sphereStaticInfo
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void freeSphereStaticInfo(struct SphereStaticInfo *sphereStaticInfo)
{
    sdword numTableEntrys = sphereStaticInfo->numTableEntries;
    sdword i;

    for (i=0;i<numTableEntrys;i++)
    {
        memFree(sphereStaticInfo->sphereTableEntryPtrs[i]);
        sphereStaticInfo->sphereTableEntryPtrs[i] = NULL;   // CAT: freeSphereStaticInfo 0x00481590 clears the entry
    }

    memFree(sphereStaticInfo);
}

/*-----------------------------------------------------------------------------
    CAT (M2-R5 command-tactics): the force-field formations of Sentinels (FORCEFIELD_WALL_FORMATION 9,
    FORCEFIELD_SPHERE_FORMATION 10). Each Sentinel's specifics hold the triangles of the field it spans with its
    neighbours (SentinelSpec.numLinks / links, Ship+0x640..); the formation code fills them as it lays the ships out.
-----------------------------------------------------------------------------*/

// CAT: the spacing of a force-field wall (cataclysm.exe 0x0087f960 = 300.0, read by
//      SetFormationOffsetsForForceFieldWall 0x00481ef0; no script key sets it; name chosen here)
real32 FORCEFIELD_WALL_SPACING = 300.0f;

// CAT: the frame a force-field sphere's offsets are turned by (cataclysm.exe 0x0072af18: the identity matrix)
static matrix catForceFieldSphereFrame = { 1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f };

static SentinelSpec *catSentinelSpec(Ship *ship)
{
    return (SentinelSpec *)ship->ShipSpecifics;
}

static void catSentinelLink(Ship *ship,sdword index,Ship *a,Ship *b)
{
    catSentinelSpec(ship)->links[index].a = a;
    catSentinelSpec(ship)->links[index].b = b;
}

/*-----------------------------------------------------------------------------
    Name        : cat_formationSphereSentinelLinks
    Description : the field triangles the Sentinel at position last - first of a force-field sphere spans
    Inputs      : spec (the Sentinel's), selection, first (the centre ship's index), last (this ship's)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: new (cat_formationSphereSentinelLinks 0x004815d0): a fixed table for the 12 places of a 13-ship sphere
void cat_formationSphereSentinelLinks(SentinelSpec *spec,SelectCommand *selection,sdword first,sdword last)
{
    Ship **s = &selection->ShipPtr[first];
    sdword n = 0;

#define CATLINK(x,y) { spec->links[n].a = s[x]; spec->links[n].b = s[y]; n++; }
    spec->numLinks = 0;
    switch (last - first)
    {
        case 1:
            return;
        case 2:  CATLINK(0,1); break;
        case 3:  CATLINK(0,2); break;
        case 4:  CATLINK(0,3); break;
        case 5:  CATLINK(0,4); CATLINK(1,0); break;
        case 6:  CATLINK(1,5); break;
        case 7:  CATLINK(1,6); CATLINK(2,1); break;
        case 8:  CATLINK(2,7); CATLINK(3,2); break;
        case 9:  CATLINK(4,3); CATLINK(3,8); break;
        case 10: CATLINK(5,4); CATLINK(4,9); CATLINK(6,5); break;
        case 11: CATLINK(6,7); CATLINK(7,8); CATLINK(8,9); CATLINK(9,10); CATLINK(10,6); break;
        default:
            return;
    }
#undef CATLINK
    spec->numLinks = n;
}

/*-----------------------------------------------------------------------------
    Name        : cat_formationSentinelFieldLevel
    Description : the force-field level of a selection of Sentinels: 2 when all have the third force-field ability,
                  1 the second, 0 otherwise; every Sentinel gets it as its charge level
    Inputs      : selection (Sentinels only)
    Outputs     :
    Return      : the number of ships
----------------------------------------------------------------------------*/
// CAT: new (cat_formationSentinelFieldLevel 0x004817d0): the abilities are CAT_UPGRADE_Ability3 / Ability2
//      (ABILITY_FORCE_FIELD3 / 2); the level goes to SentinelSpec.chargeLevel (Ship+0x964)
sdword cat_formationSentinelFieldLevel(SelectCommand *selection)
{
    sdword level = 2;
    sdword i;

    for (i = 0; i < selection->numShips; i++)
    {
        if (level == 2)
        {
            if (!cat_shipHasUpgrade(selection->ShipPtr[i],CAT_UPGRADE_Ability3))
            {
                level = 1;
                if (!cat_shipHasUpgrade(selection->ShipPtr[i],CAT_UPGRADE_Ability2))
                {
                    level = 0;
                }
            }
        }
        else if (level == 1)
        {
            if (!cat_shipHasUpgrade(selection->ShipPtr[i],CAT_UPGRADE_Ability2))
            {
                level = 0;
            }
        }
        if ((selection->ShipPtr[i]->shiptype != sSentinel) && (selection->ShipPtr[i]->shiptype != bSentinel))
        {
            dbgFatalf(DBG_Loc,"Can only have sentinels forming a spherical force field!");
        }
    }
    for (i = 0; i < selection->numShips; i++)
    {
        catSentinelSpec(selection->ShipPtr[i])->chargeLevel = level;
    }
    return i;
}

/*-----------------------------------------------------------------------------
    Name        : SetFormationOffsetsForForceFieldWall
    Description : lays a force-field wall out: the leader in the middle, then rings of 4, 8 and 12 Sentinels in the
                  leader's up/right plane, FORCEFIELD_WALL_SPACING apart, each spanning field triangles with its
                  inner neighbours
    Inputs      : command
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: new (SetFormationOffsetsForForceFieldWall 0x00481ef0); the exe's table, ring by ring
void SetFormationOffsetsForForceFieldWall(CommandToDo *command)
{
    static const struct { real32 x, y; sdword a0, b0, a1, b1; } place[25] =
    {
        // the leader, then the ring of 4
        {  0.0f,  0.0f, -1, -1, -1, -1 },
        {  1.0f,  0.0f, -1, -1, -1, -1 },
        {  0.0f,  1.0f,  1,  0, -1, -1 },
        { -1.0f,  0.0f,  2,  0, -1, -1 },
        {  0.0f, -1.0f,  3,  0,  0,  1 },
        // the ring of 8
        {  1.0f,  1.0f,  1,  2, -1, -1 },
        {  0.0f,  2.0f,  5,  2, -1, -1 },
        { -1.0f,  1.0f,  2,  3,  6,  2 },
        { -2.0f,  0.0f,  7,  3, -1, -1 },
        { -1.0f, -1.0f,  8,  3,  3,  4 },
        {  0.0f, -2.0f,  9,  4, -1, -1 },
        {  1.0f, -1.0f, 10,  4,  4,  1 },
        {  2.0f,  0.0f, 11,  1,  1,  5 },
        // the ring of 12 (clFormation caps a force-field wall at 24 ships; a WALL_FORMATION of Sentinels is not capped)
        {  2.0f,  1.0f, 12,  5, -1, -1 },
        {  1.0f,  2.0f, 13,  5,  5,  6 },
        {  0.0f,  3.0f, 14,  6, -1, -1 },
        { -1.0f,  2.0f,  6,  7, 15,  6 },
        { -2.0f,  1.0f, 16,  7,  7,  8 },
        { -3.0f,  0.0f, 17,  8, -1, -1 },
        { -2.0f, -1.0f, 18,  8,  8,  9 },
        { -1.0f, -2.0f, 19,  9,  9, 10 },
        {  0.0f, -3.0f, 20, 10, -1, -1 },
        {  1.0f, -2.0f, 21, 10, 10, 11 },
        {  2.0f, -1.0f, 22, 11, 11, 12 },
        {  3.0f,  0.0f, 23, 12, 12, 13 },
    };
    SelectCommand *selection = command->selection;
    real32 d = FORCEFIELD_WALL_SPACING;
    sdword i;
    Ship *ship;
    SentinelSpec *spec;

    cat_formationSentinelFieldLevel(selection);
    for (i = 0; i < selection->numShips; i++)
    {
        if (i >= (sdword)(sizeof(place) / sizeof(place[0])))
        {
            // the exe stops after the 25th place with a nullsub warning ("cannont handle this many sentinels!",
            // 0x00880cc0)
            return;
        }
        ship = selection->ShipPtr[i];
        spec = catSentinelSpec(ship);
        spec->numLinks = 0;
        vecSet(ship->formationOffset,place[i].x * d,place[i].y * d,0.0f);
        if (place[i].a0 >= 0)
        {
            catSentinelLink(ship,0,selection->ShipPtr[place[i].a0],selection->ShipPtr[place[i].b0]);
            spec->numLinks = 1;
        }
        if (place[i].a1 >= 0)
        {
            catSentinelLink(ship,1,selection->ShipPtr[place[i].a1],selection->ShipPtr[place[i].b1]);
            spec->numLinks = 2;
        }
    }
}

SphereTableEntry *findSphereTableEntry(sdword numShips)
{
    sdword tableindex;
    sdword numTables = sphereStaticInfo->numTableEntries;
    SphereTableEntry *tableentry = NULL;

    for (tableindex=0;tableindex<numTables;tableindex++)
    {
        tableentry = sphereStaticInfo->sphereTableEntryPtrs[tableindex];

        if (numShips <= tableentry->numShipsCanHandle)
        {
            return tableentry;
        }
    }

    dbgFatal(DBG_Loc,"\nSphere formation too big");

    // never going to get here because of the dbgFatal above; this is here just to keep the compiler happy
    return tableentry;
}

/*-----------------------------------------------------------------------------
    Name        : sphere_special_proximity_solution(selection);
    Description : retruns true if we can creativly change the sphere formation for proximity sensors
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

bool32 sphere_special_proximity_solution(Ship *centreship, SelectCommand *selection)
{
   sdword i;
   for(i=1;i<selection->numShips;i++)
   {
      // CAT: both races' proximity sensors, as the exe's inlined copy in formationSpecificsSphere tests (0x00481933: 0x47 and
      //      0x57); this HW1 helper itself has no caller
      if ((selection->ShipPtr[i]->shiptype != kProximitySensor) && (selection->ShipPtr[i]->shiptype != tProximitySensor))
      {
         //non-proximity sensor detected, return false
         return(FALSE);
      }
   }
   //Entire selection is proximity sensors, except maybe centreship

   if(centreship == selection->ShipPtr[0])
      return(TRUE);                          //not protecting anything
   else if ((selection->ShipPtr[0]->shiptype == kProximitySensor) || (selection->ShipPtr[0]->shiptype == tProximitySensor))
      return(TRUE);                          //all proximity sensors

   return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : formationSpecificsSphere
    Description : calculates and returns custom information for the specifics of a sphere formation command
    Inputs      : command, forceField (TRUE for a force-field sphere of Sentinels)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: formationSpecificsSphere 0x00481880 takes a second argument, the force-field flag. A force-field sphere uses
//      the 13-ship table entry, sizes itself from the centre ship's forceFieldProtectionDistanceScale x
//      avoidcollspheresize (no minimum radius), puts the bottom pole last, turns every offset by the exe's
//      catForceFieldSphereFrame and gives each Sentinel its field triangles (cat_formationSphereSentinelLinks) and
//      the field level (cat_formationSentinelFieldLevel). Both proximity-sensor types count; no HW1 mission-14
//      ion-cannon override. A last ship beyond the declinations goes to the bottom pole (the exe returns there).
void formationSpecificsSphere(CommandToDo *command,sdword forceField)
{
    SelectCommand *selection = command->selection;
    sdword numShips = selection->numShips;
    sdword shipWorkingOn;
    sdword centreIndex;
    real32 sphereRadius;
    real32 shipRadius,minrange;
    sdword i,j,k;
    sdword numDeclinations;
    SphereDeclination *sphereDeclination;
    vector declination;
    vector tempvec, tempvec2;
    matrix rotaboutz;
    Ship *centreship;
    sdword numShipsIncludingCentre;
    Ship *ship;
    SphereTableEntry *tableentry;
    bool32 prox_sol;

    if (forceField)
    {
        cat_formationSentinelFieldLevel(selection);
    }

    if (command->ordertype.attributes & COMMAND_MASK_PROTECTING)
    {
        centreship = command->protect->ShipPtr[0];
        centreIndex = 0;
        numShipsIncludingCentre = numShips + 1;
    }
    else if ((command->ordertype.order == COMMAND_ATTACK) && command->attack->numTargets && (command->formation.enders == TRUE))
    {
        centreship = (Ship *)command->selection->ShipPtr[0]->attackvars.attacktarget;
        if (centreship == NULL)
        {
            centreship = (Ship *)command->attack->TargetPtr[0];
        }
        centreIndex = 0;
        numShipsIncludingCentre = numShips + 1;
    }
    else
    {
        centreship = selection->ShipPtr[0];
        centreIndex = 1;
        numShipsIncludingCentre = numShips;
    }

    // all proximity sensors (of either race) but maybe the centre ship
    prox_sol = TRUE;
    for (i = 1; i < selection->numShips; i++)
    {
        if ((selection->ShipPtr[i]->shiptype != kProximitySensor) && (selection->ShipPtr[i]->shiptype != tProximitySensor))
        {
            prox_sol = FALSE;
            break;
        }
    }
    if (prox_sol && !((centreship == selection->ShipPtr[0]) || (selection->ShipPtr[0]->shiptype == kProximitySensor) ||
                      (selection->ShipPtr[0]->shiptype == tProximitySensor)))
    {
        prox_sol = FALSE;
    }

    if (numShipsIncludingCentre == 1)
    {
        return;             // if only leader is present, don't need to calculate anything
    }

    if ((prox_sol) && ((centreship->shiptype == kProximitySensor) || (centreship->shiptype == tProximitySensor)))
    {
        shipRadius = ((ProximitySensorStatics *) ((ShipStaticInfo *)(centreship->staticinfo))->custstatinfo)->SearchRadius;
    }
    else
    {
        shipRadius = centreship->staticinfo->common.staticCollInfo.collspheresize;
    }

    if (!forceField)
    {
        tableentry = findSphereTableEntry(numShipsIncludingCentre);
    }
    else
    {
        tableentry = catForceFieldSphereEntry;
    }
    numDeclinations = tableentry->numDeclinations;

    if (prox_sol)
    {
        sphereRadius = shipRadius + ((ProximitySensorStatics *) ((ShipStaticInfo *)(selection->ShipPtr[1]->staticinfo))->custstatinfo)->SearchRadius;
    }
    else if ((command->formation.enders == TRUE) && !forceField)
    {
        minrange = REALlyBig;
        for(k=1;k<numShips;k++)
        {
            if(command->selection->ShipPtr[k]->staticinfo->bulletRange[command->selection->ShipPtr[k]->tacticstype] < minrange)
                minrange = command->selection->ShipPtr[k]->staticinfo->bulletRange[command->selection->ShipPtr[k]->tacticstype];
        }
        sphereRadius = minrange*0.9f;
    }
    else if (!forceField)
    {
        if (shipRadius > SPHERESIZE_IGNORE_NUMDECLINATIONS)
            sphereRadius = shipRadius * SPHERE_FORMATION_PADDING_SCALE + shipRadius * SPHERE_FORMATION_PADDING_SCALE;
        else
            sphereRadius = (real32)((numDeclinations < 2) ? 2 : numDeclinations) * SPHERE_FORMATION_PADDING_SCALE * shipRadius;
    }
    else
    {
        sphereRadius = centreship->staticinfo->forceFieldProtectionDistanceScale *
                       centreship->staticinfo->common.staticCollInfo.avoidcollspheresize;
    }

    if ((sphereRadius >= SPHERE_MINIMUM_RADIUS) || forceField)
    {
        if (sphereRadius > SPHERE_MAXIMUM_RADIUS)
        {
            sphereRadius = SPHERE_MAXIMUM_RADIUS;
        }
    }
    else
    {
        sphereRadius = SPHERE_MINIMUM_RADIUS;
    }

    // position ship at top pole
    shipWorkingOn = centreIndex;
    ship = selection->ShipPtr[shipWorkingOn];

    vecSet(ship->formationOffset,0.0f,0.0f,sphereRadius);
    vecSet(ship->formationHeading,0.0f,0.0f,1.0f);
    if (forceField)
    {
        catSentinelSpec(ship)->numLinks = 0;
        tempvec = ship->formationOffset;
        tempvec2 = ship->formationHeading;
        matMultiplyVecByMat(&ship->formationOffset,&tempvec,&catForceFieldSphereFrame);
        matMultiplyVecByMat(&ship->formationHeading,&tempvec2,&catForceFieldSphereFrame);
    }

    shipWorkingOn++;
    if (numShips == shipWorkingOn)
    {
        return;
    }
    ship = selection->ShipPtr[shipWorkingOn];

    if (!forceField)
    {
        // position ship at bottom pole
        vecSet(ship->formationOffset,0.0f,0.0f,-sphereRadius);
        vecSet(ship->formationHeading,0.0f,0.0f,-1.0f);

        shipWorkingOn++;
        if (numShips == shipWorkingOn)
        {
            return;
        }
        ship = selection->ShipPtr[shipWorkingOn];
    }

    // now position rest of ships.  Look up the table we need for the number of ships we have

    for (i=0,sphereDeclination=&tableentry->sphereDeclinations[0];i<numDeclinations;i++,sphereDeclination++)
    {
        // get the declination vector

        vecSet(declination,sphereDeclination->cosdeclination,0.0f,sphereDeclination->sindeclination);

        matMakeRotAboutZ(&rotaboutz,sphereDeclination->cosrotang,sphereDeclination->sinrotang);

        for (j=0;j<sphereDeclination->numAnglePoints;j++)
        {
            // place the position

            ship->formationHeading = declination;
            vecScalarMultiply(ship->formationOffset,declination,sphereRadius);
            if (forceField)
            {
                tempvec = ship->formationOffset;
                tempvec2 = ship->formationHeading;
                matMultiplyVecByMat(&ship->formationOffset,&tempvec,&catForceFieldSphereFrame);
                matMultiplyVecByMat(&ship->formationHeading,&tempvec2,&catForceFieldSphereFrame);
                cat_formationSphereSentinelLinks(catSentinelSpec(ship),selection,centreIndex,shipWorkingOn);
            }

            shipWorkingOn++;
            if (shipWorkingOn < numShips)
            {
                ship = selection->ShipPtr[shipWorkingOn];     // (the exe reads the next slot before its checks)
            }
            if ((shipWorkingOn == numShips - 1) && (i == numDeclinations - 1) && (j == sphereDeclination->numAnglePoints - 1))
            {
                // the last ship goes to the bottom pole (0x00481d4b)
                vecSet(ship->formationOffset,0.0f,0.0f,-sphereRadius);
                vecSet(ship->formationHeading,0.0f,0.0f,-1.0f);
                if (!forceField)
                {
                    return;
                }
                cat_formationSphereSentinelLinks(catSentinelSpec(ship),selection,centreIndex,shipWorkingOn);
                tempvec = ship->formationOffset;
                tempvec2 = ship->formationHeading;
                matMultiplyVecByMat(&ship->formationOffset,&tempvec,&catForceFieldSphereFrame);
                matMultiplyVecByMat(&ship->formationHeading,&tempvec2,&catForceFieldSphereFrame);
                return;
            }
            if (numShips == shipWorkingOn)
            {
                return;
            }

            tempvec = declination;
            matMultiplyMatByVec(&declination,&rotaboutz,&tempvec);
        }
    }
}

// define for a ship moving into formation, without changing heading, and trying to steady formation
#define FormationShipJustMoveSteady(desiredpos) \
    if (MoveReachedDestinationVariable(ship,&desiredpos,100.0f))                                                                                                                                                                \
    {                                                                                                                                                                                                                           \
        ship->header.flags |= SOF_DontDrawTrails;                                                                                                                                                                                      \
    }                                                                                                                                                                                                                           \
    if (!MoveReachedDestinationVariable(ship,&desiredpos,10.0f))                                                                                                                                                                \
    {                                                                                                                                                                                                                           \
        if (aishipFlyToPointAvoidingObjsWithVel(ship,&desiredpos,AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured|AISHIP_IgnoreFormationObscuredPoints|AISHIP_FastAsPossible,0.0f,&leader->obj.posinfo.velocity) & AISHIP_FLY_OBJECT_IN_WAY)             \
        {                                                                                                                                                                                                                       \
            aitrackSteadyShipDriftOnly(ship);                                                                                                           \
        }                                                                                                                                                                                                                       \
    }                                                                                                                                                   \
    else                                                                                                                                                \
    {                                                                                                                                                   \
        aitrackSteadyShipDriftOnly(ship);                                                                                                               \
    }

// define for a ship moving into formation, without changing heading
#define FormationShipJustMove(desiredpos) \
    if (!MoveReachedDestinationVariable(ship,&desiredpos,10.0f))                                                                                                                                                                \
    {                                                                                                                                                                                                                           \
        aishipFlyToPointAvoidingObjsWithVel(ship,&desiredpos,AISHIP_FastAsPossible,0.0f,&leader->obj.posinfo.velocity);             \
    }

// define for a ship moving into formation, without changing heading, and not trying to fly to obscured points
#define FormationShipJustMoveNotToObscuredPoints(desiredpos) \
    if (!MoveReachedDestinationVariable(ship,&desiredpos,10.0f))                                                                                                                                                                \
    {                                                                                                                                                                                                                           \
        if (aishipFlyToPointAvoidingObjsWithVel(ship,&desiredpos,AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured|AISHIP_IgnoreFormationObscuredPoints|AISHIP_FastAsPossible,0.0f,&leader->obj.posinfo.velocity) & AISHIP_FLY_OBJECT_IN_WAY)             \
        {                                                                                                                                                                                                                                                            \
            aitrackSteadyShipDriftOnly(ship);                                                                                                           \
        }                                                                                                                                                                                                                                                            \
    }

// CAT: HW1's FormationShipMoveToSteady / FormationShipMoveToNotToObscuredPoints macros are functions in Cataclysm
//      (cat_formationShipMoveToSteady 0x00482620, cat_formationShipMoveToNotToObscuredPoints 0x00482770); the
//      capital-ship test is isCapitalShipStaticOrBig's, inline
void cat_formationShipMoveToSteady(Ship *ship,Ship *leader,vector *desiredpos,vector *head)
{
    ShipStaticInfo *shipstatic = (ShipStaticInfo *)ship->staticinfo;
    real32 tmpdistaway = MoveLeftToGo(ship,desiredpos);
    udword tmpuseflags = AISHIP_PointInDirectionFlying | AISHIP_FastAsPossible|AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured|AISHIP_IgnoreFormationObscuredPoints;

    if (isCapitalShipStaticOrBig(shipstatic))
    {
        if (tmpdistaway <= 500.0f)
        {
            aitrackHeadingWithBank(ship,head,FLYSHIP_HEADINGACCURACY,shipstatic->shipOrDerelict.sinbank);
            tmpuseflags = AISHIP_FastAsPossible|AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured|AISHIP_IgnoreFormationObscuredPoints;
            if (tmpdistaway <= 100.0f) ship->header.flags |= SOF_DontDrawTrails;
        }
    }
    else if (tmpdistaway <= 100.0f)
    {
        aitrackHeadingWithBank(ship,head,FLYSHIP_HEADINGACCURACY,shipstatic->shipOrDerelict.sinbank);
        tmpuseflags = AISHIP_FastAsPossible|AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured|AISHIP_IgnoreFormationObscuredPoints;
        ship->header.flags |= SOF_DontDrawTrails;
    }

    if (tmpdistaway >= 10.0f)
    {
        if (!(aishipFlyToPointAvoidingObjsWithVel(ship,desiredpos,tmpuseflags,0.0f,&leader->obj.posinfo.velocity) & AISHIP_FLY_OBJECT_IN_WAY))
        {
            return;
        }
        if (tmpuseflags & AISHIP_PointInDirectionFlying)
        {
            aitrackHeadingWithBank(ship,head,FLYSHIP_HEADINGACCURACY,shipstatic->shipOrDerelict.sinbank);
        }
    }
    aitrackSteadyShipDriftOnly(ship);
}

void cat_formationShipMoveToNotToObscuredPoints(Ship *ship,Ship *leader,vector *desiredpos,vector *head,bool32 pointInDirection)
{
    ShipStaticInfo *shipstatic = (ShipStaticInfo *)ship->staticinfo;
    real32 tmpdistaway = MoveLeftToGo(ship,desiredpos);
    udword tmpuseflags = AISHIP_FastAsPossible|AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured|AISHIP_IgnoreFormationObscuredPoints;

    if (pointInDirection)
    {
        tmpuseflags |= AISHIP_PointInDirectionFlying;
    }

    if (isCapitalShipStaticOrBig(shipstatic))
    {
        if (tmpdistaway <= 500.0f)
        {
            aitrackHeadingWithBank(ship,head,FLYSHIP_HEADINGACCURACY,shipstatic->shipOrDerelict.sinbank);
            tmpuseflags = AISHIP_FastAsPossible|AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured|AISHIP_IgnoreFormationObscuredPoints;
        }
    }
    else if (tmpdistaway <= 100.0f)
    {
        aitrackHeadingWithBank(ship,head,FLYSHIP_HEADINGACCURACY,shipstatic->shipOrDerelict.sinbank);
        tmpuseflags = AISHIP_FastAsPossible|AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured|AISHIP_IgnoreFormationObscuredPoints;
    }

    if (tmpdistaway >= 10.0f)
    {
        if (aishipFlyToPointAvoidingObjsWithVel(ship,desiredpos,tmpuseflags,0.0f,&leader->obj.posinfo.velocity) & AISHIP_FLY_OBJECT_IN_WAY)
        {
            if (tmpuseflags & AISHIP_PointInDirectionFlying)
            {
                aitrackHeadingWithBank(ship,head,FLYSHIP_HEADINGACCURACY,shipstatic->shipOrDerelict.sinbank);
            }
            aitrackSteadyShipDriftOnly(ship);
        }
    }
}

void formationWingmanTrackLeader(struct Ship *ship,struct Ship *leader,bool32 rotate)
{
    ShipStaticInfo *shipstatic = (ShipStaticInfo *)ship->staticinfo;
    vector heading;
    vector right;
    vector rightback;
    vector desiredpos;
    real32 sepdist;

    matGetVectFromMatrixCol2(right,leader->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol3(heading,leader->rotatable.rotinfo.coordsys);

    sepdist = leader->staticinfo->common.staticCollInfo.collspheresize;
    sepdist += ship->staticinfo->common.staticCollInfo.collspheresize;
    sepdist *= DELTA_FORMATION_PADDING_SCALE;

    vecSub(rightback,right,heading);
    vecMultiplyByScalar(rightback,sepdist);

    vecAdd(desiredpos,leader->obj.posinfo.position,rightback);

//    FormationShipMoveTo(desiredpos,heading);
    if (rotate)
    {
        real32 tmpdistaway = MoveLeftToGo(ship,&desiredpos);
        udword tmpuseflags = AISHIP_PointInDirectionFlying | AISHIP_FastAsPossible;

        if (tmpdistaway <= 100.0f)
        {
            aitrackHeadingWithBank(ship,&(heading),FLYSHIP_HEADINGACCURACY,shipstatic->shipOrDerelict.sinbank);
            tmpuseflags = AISHIP_FastAsPossible;
        }

        if (tmpdistaway >= 10.0f)
        {
            aishipFlyToPointAvoidingObjsWithVel(ship,&desiredpos,tmpuseflags,0.0f,&leader->obj.posinfo.velocity);
        }
    }
    else
    {
        if (!MoveReachedDestinationVariable(ship,&desiredpos,10.0f))
        {
            aishipFlyToPointAvoidingObjsWithVel(ship,&desiredpos,AISHIP_FastAsPossible,0.0f,&leader->obj.posinfo.velocity);
        }
    }
}

//function to determine if selection contains nothing but proximity sensors
// CAT: FormationCalculateOffsets 0x00483759 asks each ship to be a kProximitySensor and a tProximitySensor at once
//      (jne 0x47; jne 0x57), so only an empty selection passes; the HW1 TargetDrone wall solution is gone
bool32 need_Proximity_Sensor_Solution(SelectCommand *selection)
{
   sdword i;
   for(i=0;i<selection->numShips;i++)
   {
      if((selection->ShipPtr[i]->shiptype != kProximitySensor) || (selection->ShipPtr[i]->shiptype != tProximitySensor))
      {
         //non-proximity sensor detected, return false
         return(FALSE);
      }
   }

   //Entire selection is proximity sensors
   return(TRUE);
}


#define oneOverRootTwo 0.7071068f

void setFormationToDo(struct CommandToDo *formationtodo)
{
    sdword i;
    SelectCommand *selection = formationtodo->selection;
    sdword numShips = selection->numShips;
    Ship *leader = selection->ShipPtr[0];
    matrix *coordsys;
    vector position;
    vector desiredposition;
    Ship *ship;
    ShipStaticInfo *shipstatic;
    vector heading;

    formationtodo->formation.percentmaxspeed = FORMATION_TRAVELVEL_MAXSCALE;

    dbgAssertOrIgnore(numShips > 0);
    if (numShips == 1)
    {
        return;
    }

    position = leader->impactable.collInfo.collPosition;     // CAT: the leader's collision centre (setFormationToDo 0x00482a28; HW1: its position)

    /*  Setup formations coordinate system */
    if(formationtodo->formation.formationLocked)
    {
        coordsys = &formationtodo->formation.coordsys;
        matGetVectFromMatrixCol3(heading,formationtodo->formation.coordsys);
    }
    else
    {
        coordsys = &leader->rotatable.rotinfo.coordsys;
        matGetVectFromMatrixCol3(heading,leader->rotatable.rotinfo.coordsys);
    }

    // CAT: setFormationToDo 0x00482a00: the flat formations (with the force-field wall) place each ship through
    //      cat_univSetShipPosition; the spheres (with the force-field sphere) set the position directly; no assertion
    switch (formationtodo->formation.formationtype)
    {
        case DELTA_FORMATION:
        case BROAD_FORMATION:
        case DELTA3D_FORMATION:
        case CLAW_FORMATION:
        case WALL_FORMATION:
        case CUSTOM_FORMATION:
        case FORCEFIELD_WALL_FORMATION:
            for (i=1;i<numShips;i++)
            {
                ship = selection->ShipPtr[i];
                shipstatic = (ShipStaticInfo *)ship->staticinfo;

                matMultiplyMatByVec(&desiredposition,coordsys,&ship->formationOffset);
                vecAddTo(desiredposition,position);

                cat_univSetShipPosition(ship,&desiredposition);
                ship->rotatable.rotinfo.coordsys = *coordsys;
                univUpdateObjRotInfo((SpaceObjRot *)ship);
            }
            break;

        case SPHERE_FORMATION:
        case FORCEFIELD_SPHERE_FORMATION:
            for (i=1;i<numShips;i++)
            {
                ship = selection->ShipPtr[i];
                shipstatic = (ShipStaticInfo *)ship->staticinfo;

                vecAdd(ship->obj.posinfo.position,position,ship->formationOffset);
                ship->rotatable.rotinfo.coordsys = *coordsys;
                univUpdateObjRotInfo((SpaceObjRot *)ship);
            }
            break;

        default:
            break;
    }
    (void)shipstatic;
    (void)heading;
}

// CAT: a formation ship that flies to its place and turns to heading (HW1's FormationShipMoveTo macro, inline in
//      processFormationToDo 0x00482e16 / 0x004831dc and processMilitaryParadeToDo)
static void catFormationShipMoveTo(Ship *ship,Ship *leader,vector *desiredpos,vector *head)
{
    ShipStaticInfo *shipstatic = (ShipStaticInfo *)ship->staticinfo;
    real32 tmpdistaway = MoveLeftToGo(ship,desiredpos);
    udword tmpuseflags = AISHIP_PointInDirectionFlying | AISHIP_FastAsPossible;

    if (isCapitalShipStaticOrBig(shipstatic))
    {
        if (tmpdistaway <= 500.0f)
        {
            aitrackHeadingWithBank(ship,head,FLYSHIP_HEADINGACCURACY,shipstatic->shipOrDerelict.sinbank);
            tmpuseflags = AISHIP_FastAsPossible;
        }
    }
    else if (tmpdistaway <= 100.0f)
    {
        aitrackHeadingWithBank(ship,head,FLYSHIP_HEADINGACCURACY,shipstatic->shipOrDerelict.sinbank);
        tmpuseflags = AISHIP_FastAsPossible;
    }

    if (tmpdistaway >= 10.0f)
    {
        aishipFlyToPointAvoidingObjsWithVel(ship,desiredpos,tmpuseflags,0.0f,&leader->obj.posinfo.velocity);
    }
}

// CAT: processFormationToDo 0x00482b90: the formation to update is the leader's command (Cataclysm has no
//      Ship.formationcommand); a force-field wall is laid out like the other flat formations, a force-field sphere
//      like a sphere but turned with the centre ship; the HW1 move macros are cat_formationShipMoveToSteady /
//      cat_formationShipMoveToNotToObscuredPoints.
void processFormationToDo(struct CommandToDo *formationtodo,bool32 steadyFormation,bool32 passiveAttacked)
{
    sdword i;
    SelectCommand *selection = formationtodo->selection;
    sdword numShips = selection->numShips;
    Ship *leader = selection->ShipPtr[0];
    matrix *coordsys;
    vector position;
    vector desiredposition;
    bool32 dontrotate;
    Ship *ship;
    ShipStaticInfo *shipstatic;
    vector heading;
    real32 error = 0;
    vector errorvect;
    bool32 calcError = (UNIVERSE_WOODPECKER(FORMATION_ERROR_CALCULATE_RATE, FORMATION_ERROR_CALCULATE_FRAME));
    // write to the leader's command instead of formationtodo - formationtodo might be a fakeCommand from delegateCommand
    CommandToDo *formationtomodify = leader->command;

    if (numShips == 1)
    {
        formationtomodify->formation.percentmaxspeed = FORMATION_TRAVELVEL_MAXSCALE;
        formationtomodify->formation.travelvel = GetShipsTravelVel(formationtomodify->selection,formationtomodify->formation.percentmaxspeed);
        return;
    }

    position = leader->impactable.collInfo.collPosition;     // CAT: the leader's collision centre (processFormationToDo 0x00482bf3; HW1: its position)

    /*  Setup formations coordinate system */
    if(formationtodo->formation.formationLocked)
    {
        coordsys = &formationtodo->formation.coordsys;
        matGetVectFromMatrixCol3(heading,formationtodo->formation.coordsys);
    }
    else
    {
        coordsys = &leader->rotatable.rotinfo.coordsys;
        matGetVectFromMatrixCol3(heading,leader->rotatable.rotinfo.coordsys);
    }

    switch (formationtodo->formation.formationtype)
    {
        case DELTA_FORMATION:
        case BROAD_FORMATION:
        case DELTA3D_FORMATION:
        case CLAW_FORMATION:
        case WALL_FORMATION:
        case CUSTOM_FORMATION:
        case FORCEFIELD_WALL_FORMATION:
            for (i=1;i<numShips;i++)
            {
                ship = selection->ShipPtr[i];
                shipstatic = (ShipStaticInfo *)ship->staticinfo;
                dontrotate = (passiveAttacked & (bool32)shipstatic->rotateToRetaliate);
                ship->shipidle = FALSE;

                matMultiplyMatByVec(&desiredposition,coordsys,&ship->formationOffset);
                vecAddTo(desiredposition,position);

                if (calcError)
                {
                    vecSub(errorvect,desiredposition,ship->obj.posinfo.position);
                    error += vecMagnitudeSquared(errorvect);
                }

                if(!bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_BrokenFormation))
                {
                    if (dontrotate)
                    {
                        if (steadyFormation)
                        {
                            FormationShipJustMoveSteady(desiredposition)
                        }
                        else
                        {
                            FormationShipJustMove(desiredposition)
                        }
                    }
                    else
                    {
                        if (steadyFormation)
                        {
                            cat_formationShipMoveToSteady(ship,leader,&desiredposition,&heading);
                        }
                        else
                        {
                            catFormationShipMoveTo(ship,leader,&desiredposition,&heading);
                        }
                    }
                }
            }
            break;

        case SPHERE_FORMATION:
        case FORCEFIELD_SPHERE_FORMATION:
        {
            bool32 protectsurround = FALSE;

            // CAT: the sphere is centred on the surrounded ship's collision centre (collInfo.collPosition, Ship+0xE0;
            //      processFormationToDo 0x00482eea / 0x00482f55), where HW1 took its position; the Sentinels' in-position
            //      test measures from the same point (cat_sentinelFormationUpdate 0x005c6830)
            if (formationtodo->ordertype.attributes & COMMAND_MASK_PROTECTING)
            {
                leader = formationtodo->protect->ShipPtr[0];
                position = leader->impactable.collInfo.collPosition;       // override position
                matGetVectFromMatrixCol3(heading,leader->rotatable.rotinfo.coordsys);
                protectsurround = TRUE;
                i = 0;
            }
            else if ((formationtodo->ordertype.order == COMMAND_ATTACK) && (formationtodo->formation.enders == TRUE))
            {
                Ship *protectship = (Ship *)formationtodo->selection->ShipPtr[0]->attackvars.attacktarget;
                if (protectship == NULL)
                {
                    protectship = (Ship*)formationtodo->attack->TargetPtr[0];
                }
                leader = protectship;
                position = protectship->impactable.collInfo.collPosition;       // override position (CAT: as above)
                protectsurround = TRUE;
                i = 0;
            }
            else
            {
                i = 1;
            }

            for (;i<numShips;i++)
            {
                ship = selection->ShipPtr[i];
                shipstatic = (ShipStaticInfo *)ship->staticinfo;
                dontrotate = (passiveAttacked & (bool32)shipstatic->rotateToRetaliate);
                if ((i & 7) != 0)
                {
                    ship->shipidle = FALSE;     // don't see shipidle for every 8th ship, and never set the 0th ship's shipidle
                }

                if (formationtodo->formation.formationtype == FORCEFIELD_SPHERE_FORMATION)
                {
                    // CAT: the force field turns with its centre ship (0x00482fa6)
                    matMultiplyMatByVec(&desiredposition,&leader->rotatable.rotinfo.coordsys,&ship->formationOffset);
                    vecAddTo(desiredposition,position);
                }
                else
                {
                    vecAdd(desiredposition,position,ship->formationOffset);
                }

                if (calcError)
                {
                    vecSub(errorvect,desiredposition,ship->obj.posinfo.position);
                    error += vecMagnitudeSquared(errorvect);
                }

                if(!bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_BrokenFormation))
                {
                    if (dontrotate)
                    {
                        if (steadyFormation)
                        {
                            FormationShipJustMoveSteady(desiredposition)
                        }
                        else
                        {
                            if (protectsurround)
                            {
                                FormationShipJustMoveNotToObscuredPoints(desiredposition)
                            }
                            else
                            {
                                FormationShipJustMove(desiredposition)
                            }
                        }
                    }
                    else
                    {
                        if (steadyFormation)
                        {
                            cat_formationShipMoveToSteady(ship,leader,&desiredposition,&ship->formationHeading);
                        }
                        else
                        {
                            if (protectsurround)
                            {
                                if (formationtodo->formation.formationtype == FORCEFIELD_SPHERE_FORMATION)
                                {
                                    vector faceheading;

                                    matMultiplyMatByVec(&faceheading,&leader->rotatable.rotinfo.coordsys,&ship->formationHeading);
                                    cat_formationShipMoveToNotToObscuredPoints(ship,leader,&desiredposition,&faceheading,TRUE);
                                }
                                else if (!IS_MOVING_LINEARLY(leader->obj.posinfo.isMoving))
                                {
                                    cat_formationShipMoveToNotToObscuredPoints(ship,leader,&desiredposition,&ship->formationHeading,TRUE);
                                }
                                else
                                {
                                    cat_formationShipMoveToNotToObscuredPoints(ship,leader,&desiredposition,&heading,TRUE);
                                }
                            }
                            else
                            {
                                catFormationShipMoveTo(ship,leader,&desiredposition,&heading);
                            }
                        }
                    }
                }
            }

            aishipTempDisableAvoiding = FALSE;
        }
        break;

        default:
            break;
    }

    if (calcError)
    {
        error /= numShips;

        if (error > FORMATION_ERROR_BIG)
        {
            formationtomodify->formation.percentmaxspeed = FORMATION_TRAVELVEL_MINSCALE;
        }
        else
        {
            if (error < FORMATION_ERROR_NEGLIGIBLE)
            {
                formationtomodify->formation.percentmaxspeed = FORMATION_TRAVELVEL_MAXSCALE;
            }
            else
            {
                formationtomodify->formation.percentmaxspeed = FORMATION_TRAVELVEL_MAXSCALE - (error / FORMATION_ERROR_BIG) * (FORMATION_TRAVELVEL_MAXSCALE - FORMATION_TRAVELVEL_MINSCALE);
            }
        }

        formationtomodify->formation.travelvel = GetShipsTravelVel(formationtomodify->selection,formationtomodify->formation.percentmaxspeed);
    }
}

void formationContentHasChanged(struct CommandToDo *command)
{
    FormationCalculateOffsets(command);
}

/*-----------------------------------------------------------------------------
    Name        : clSelectionAlreadyInFormation
    Description : Call this function to see if this selection is already in formation
    Inputs      :
    Outputs     :
    Return      : returns NO_FORMATION if this selection is not already in formation, otherwise
                  returns the type of formation the selection is in.
----------------------------------------------------------------------------*/
TypeOfFormation clSelectionAlreadyInFormation(CommandLayer *comlayer,SelectCommand *selectcom)
{
    CommandToDo *alreadyformation;

    if ((alreadyformation = IsSelectionAlreadyDoingSomething(comlayer,selectcom)) != NULL)
    {
        if (alreadyformation->ordertype.attributes & COMMAND_MASK_FORMATION)
        {
            return alreadyformation->formation.formationtype;
        }
    }
    return NO_FORMATION;
}

// CAT: no FillInShipFormationStuff: the exe has a nullsub in its place (Cataclysm has no Ship.formationcommand)

/*-----------------------------------------------------------------------------
    Name        : GetShipsTravelVel
    Description : returns the selection's velocity at which it can all travel, scaled by scalevel
    Inputs      : selection, scalevel
    Outputs     :
    Return      : returns the selection's velocity at which it can all travel, scaled by scalevel
----------------------------------------------------------------------------*/
// CAT: GetShipsTravelVel 0x004833d0: each ship's speed is its maximum velocity times its experience speed multiplier;
//      a Sentinel holding a force field goes at the speed of the ship it protects; an escort limit
//      (Ship.catProtectedSpeedLimit, cat_univLimitProtectedShipSpeed) caps the others
real32 GetShipsTravelVel(SelectCommand *selection,real32 scalevel)
{
    real32 minvel = 1e21f;
    // CAT-FIX: the exe leaves its speed local uninitialised for a field Sentinel without a protected ship (it keeps
    //      the previous ship's value, or stack garbage for the first ship); starting at the minimum's start value keeps
    //      such a ship from lowering it
    real32 testvel = 1e21f;
    sdword i;
    Ship *ship;
    bool32 fieldSentinel;

    for (i=0;i<selection->numShips;i++)
    {
        ship = selection->ShipPtr[i];
        fieldSentinel = FALSE;
        if (((ship->shiptype == sSentinel) || (ship->shiptype == bSentinel)) && cat_shipIsSentinelFieldActive(ship))
        {
            fieldSentinel = TRUE;
            if ((ship->command != NULL) && (ship->command->protect != NULL) && (ship->command->protect->numShips > 0))
            {
                testvel = cat_shipGetSpeedMultiplier(ship->command->protect->ShipPtr[0]) * tacticsGetShipsMaxVelocity(ship->command->protect->ShipPtr[0]);
            }
        }
        else
        {
            testvel = tacticsGetShipsMaxVelocity(ship) * cat_shipGetSpeedMultiplier(ship);
        }

        if (testvel < minvel)
        {
            minvel = testvel;
        }
        if (!fieldSentinel && (0.0f < ship->catProtectedSpeedLimit) && (ship->catProtectedSpeedLimit < minvel))
        {
            minvel = ship->catProtectedSpeedLimit;
        }
    }

    return minvel * scalevel;
}

void  setFormationTravelVelocity(CommandToDo *formationcommand)
{
    //won't hurt if we were to set the travel velocity and ships aren't
    //in formation, but lets not let it happen anyways.
    dbgAssertOrIgnore(formationcommand->ordertype.attributes & COMMAND_MASK_FORMATION);

    formationcommand->formation.travelvel = GetShipsTravelVel(formationcommand->selection,formationcommand->formation.percentmaxspeed);
}


/*-----------------------------------------------------------------------------
    Name        : formationRemoveShipFromSelection
    Description : removes ship from selection of formation
    Inputs      :
    Outputs     :
    Return      : -1 if not found, shipindex if found
----------------------------------------------------------------------------*/
sdword formationRemoveShipFromSelection(struct CommandToDo *formationtodo,Ship *removeship)
{
    SelectCommand *selection = formationtodo->selection;
    sdword i,j;
    sdword returnval;

    dbgAssertOrIgnore(formationtodo->ordertype.attributes & COMMAND_MASK_FORMATION);

    for (i=0;i<selection->numShips; )
    {
        if (removeship == selection->ShipPtr[i])
        {
            goto foundshiptoremove;
        }
        i++;
    }

    return -1;

foundshiptoremove:
    returnval = i;

    switch (formationtodo->formation.formationtype)
    {
        case DELTA_FORMATION:
        case BROAD_FORMATION:
            if (selection->numShips <= 2)
            {
                goto defaultremove;
            }

            // move one wing forward
            for (j=i;j<selection->numShips;j+=2)
            {
                if ((j+2) < selection->numShips)
                {
                    selection->ShipPtr[j] = selection->ShipPtr[j+2];
                }
                else if ((j+1) < selection->numShips)
                {
                    selection->ShipPtr[j] = selection->ShipPtr[j+1];
                }
            }
            selection->numShips--;
            break;

        case DELTA3D_FORMATION:
        case CLAW_FORMATION:
            // move one wing forward
            for (j=i;j<selection->numShips;j+=4)
            {
                if ((j+4) < selection->numShips)
                {
                    selection->ShipPtr[j] = selection->ShipPtr[j+4];
                }
                else if ((j+3) < selection->numShips)
                {
                    selection->ShipPtr[j] = selection->ShipPtr[j+3];
                }
                else if ((j+2) < selection->numShips)
                {
                    selection->ShipPtr[j] = selection->ShipPtr[j+2];
                }
                else if ((j+1) < selection->numShips)
                {
                    selection->ShipPtr[j] = selection->ShipPtr[j+1];
                }
            }
            selection->numShips--;
            break;

defaultremove:
        default:
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
            break;
    }

    return returnval;
}

void FormationCalculateOffsets(struct CommandToDo *formationtodo)
{
    SelectCommand *selection = formationtodo->selection;
    sdword numShips = selection->numShips;
    Ship *leader = NULL;
    ShipStaticInfo *leaderstatic = NULL;
    real32 tacticsFormationPadding = 0.0;
    sdword i = 0;
    Ship *ship = NULL;
    ShipStaticInfo *shipstatic = NULL;
    sdword prox_sol = 0;
    real32 searchRadius = 0.0;

    // CAT: FormationCalculateOffsets 0x00483620: a force-field sphere is laid out by formationSpecificsSphere and its
    //      Sentinels are disabled (reason 5: they take no orders while they hold the field); every other formation
    //      clears that reason; a wall of Sentinels only, and the force-field wall, are force-field walls
    if (formationtodo->formation.formationtype == FORCEFIELD_SPHERE_FORMATION)
    {
        formationSpecificsSphere(formationtodo,TRUE);
        for (i = 0; i < selection->numShips; i++)
        {
            cat_shipSetDisableReason(selection->ShipPtr[i],TRUE,5);
        }
        return;
    }
    for (i = 0; i < selection->numShips; i++)
    {
        if (selection->ShipPtr[i]->shipOrDerelict.catDisableReason[5] == 1)
        {
            cat_shipSetDisableReason(selection->ShipPtr[i],FALSE,5);
        }
    }
    if (formationtodo->formation.formationtype == WALL_FORMATION)
    {
        bool32 allSentinels = TRUE;

        for (i = 0; i < numShips; i++)
        {
            if ((selection->ShipPtr[i]->shiptype != sSentinel) && (selection->ShipPtr[i]->shiptype != bSentinel))
            {
                allSentinels = FALSE;
            }
        }
        if (allSentinels)
        {
            SetFormationOffsetsForForceFieldWall(formationtodo);
            return;
        }
    }
    if (formationtodo->formation.formationtype == SPHERE_FORMATION)
    {
        formationSpecificsSphere(formationtodo,FALSE);
        return;
    }
    if (formationtodo->formation.formationtype == FORCEFIELD_WALL_FORMATION)
    {
        SetFormationOffsetsForForceFieldWall(formationtodo);
        return;
    }

    leader = selection->ShipPtr[0];
    leaderstatic = leader->staticinfo;
    tacticsFormationPadding = tacticsInfo.formationPadding[formationtodo->formation.tacticalState];

    prox_sol = need_Proximity_Sensor_Solution(selection);
    if (prox_sol)
    {
        searchRadius = ((ProximitySensorStatics *)leaderstatic->custstatinfo)->SearchRadius;
    }

    switch (formationtodo->formation.formationtype)
    {
        case DELTA_FORMATION:
        {
            // offset in format up,right,heading for x,y,z
            // rightback is (0,1,-1)
            // leftback is (0,-1,-1)
            real32 rightsepdist;
            real32 leftsepdist;
            real32 thissepdist;
            vector desiredposright = { 0.0f, 0.0f, 0.0f };
            vector desiredposleft = { 0.0f, 0.0f, 0.0f };

            if (prox_sol)
            {
                leftsepdist = searchRadius;
            }
            else
            {
                leftsepdist = leaderstatic->common.staticCollInfo.collspheresize*leaderstatic->formationPaddingModifier*tacticsFormationPadding;
            }
            rightsepdist = leftsepdist;

            for (i=1;i<numShips;i++)
            {
                ship = selection->ShipPtr[i];
                shipstatic = (ShipStaticInfo *)ship->staticinfo;

                if (prox_sol)
                {
                    thissepdist = searchRadius;
                }
                else
                {
                    thissepdist = shipstatic->common.staticCollInfo.collspheresize*shipstatic->formationPaddingModifier*tacticsFormationPadding;
                }

                if (i & 1)
                {
                    rightsepdist += thissepdist;
                    if (!prox_sol) rightsepdist *= DELTA_FORMATION_PADDING_SCALE;

                    if(formationtodo->formation.flipState)
                        desiredposright.y -= rightsepdist;
                    else
                        desiredposright.y += rightsepdist;

                    desiredposright.z -= rightsepdist;

                    ship->formationOffset = desiredposright;

                    rightsepdist = thissepdist;
                }
                else
                {
                    leftsepdist += thissepdist;
                    if (!prox_sol) leftsepdist *= DELTA_FORMATION_PADDING_SCALE;

                    if(formationtodo->formation.flipState)
                        desiredposleft.y += leftsepdist;
                    else
                        desiredposleft.y -= leftsepdist;

                    desiredposleft.z -= leftsepdist;

                    ship->formationOffset = desiredposleft;

                    leftsepdist = thissepdist;
                }
            }
        }
        break;

        case BROAD_FORMATION:
        {
            // offset in format up,right,heading for x,y,z
            // right is (0,1,0)
            // left is (0,-1,0)
            real32 rightsepdist;
            real32 leftsepdist;
            real32 thissepdist;
            vector desiredposright = { 0.0f, 0.0f, 0.0f };
            vector desiredposleft = { 0.0f, 0.0f, 0.0f };

            if (prox_sol)
            {
                leftsepdist = searchRadius;
            }
            else
            {
                leftsepdist = leaderstatic->common.staticCollInfo.collspheresize*leaderstatic->formationPaddingModifier*tacticsFormationPadding;
            }
            rightsepdist = leftsepdist;

            for (i=1;i<numShips;i++)
            {
                ship = selection->ShipPtr[i];
                shipstatic = (ShipStaticInfo *)ship->staticinfo;

                if (prox_sol)
                {
                    thissepdist = searchRadius;
                }
                else
                {
                    thissepdist = shipstatic->common.staticCollInfo.collspheresize*shipstatic->formationPaddingModifier*tacticsFormationPadding;
                }

                if (i & 1)
                {
                    rightsepdist += thissepdist;
                    if (!prox_sol) rightsepdist *= BROAD_FORMATION_PADDING_SCALE;

                    if(formationtodo->formation.flipState)
                        desiredposright.y -= rightsepdist;
                    else
                        desiredposright.y += rightsepdist;

                    ship->formationOffset = desiredposright;

                    rightsepdist = thissepdist;
                }
                else
                {
                    leftsepdist += thissepdist;
                    if (!prox_sol) leftsepdist *= BROAD_FORMATION_PADDING_SCALE;

                    if(formationtodo->formation.flipState)
                        desiredposleft.y += leftsepdist;
                    else
                        desiredposleft.y -= leftsepdist;



                    ship->formationOffset = desiredposleft;

                    leftsepdist = thissepdist;
                }
            }
        }
        break;

        case DELTA3D_FORMATION:
        {
            // offset in format up,right,heading for x,y,z
            vector desiredposupright = { 0.0f, 0.0f, 0.0f };
            vector desiredposupleft = { 0.0f, 0.0f, 0.0f };
            vector desiredposdnright = { 0.0f, 0.0f, 0.0f };
            vector desiredposdnleft = { 0.0f, 0.0f, 0.0f };
            real32 uprightsepdist;
            real32 upleftsepdist;
            real32 dnrightsepdist;
            real32 dnleftsepdist;
            real32 thissepdist;

            if (prox_sol)
            {
                uprightsepdist = searchRadius;
            }
            else
            {
                uprightsepdist = leaderstatic->common.staticCollInfo.collspheresize*leaderstatic->formationPaddingModifier*tacticsFormationPadding;
            }

            upleftsepdist = uprightsepdist;
            dnrightsepdist = uprightsepdist;
            dnleftsepdist = uprightsepdist;

            for (i=1;i<numShips;i++)
            {
                ship = selection->ShipPtr[i];
                shipstatic = (ShipStaticInfo *)ship->staticinfo;

                if (prox_sol)
                {
                    thissepdist = searchRadius;
                }
                else
                {
                    thissepdist = shipstatic->common.staticCollInfo.collspheresize*shipstatic->formationPaddingModifier*tacticsFormationPadding;
                }

                switch (i & 3)
                {
                    case 1:
                        uprightsepdist += thissepdist;
                        if (!prox_sol) uprightsepdist *= DELTA3D_FORMATION_PADDING_SCALE;

                        // uprightback is (1,1,-1)
                        desiredposupright.x += uprightsepdist;

                        if(formationtodo->formation.flipState)
                            desiredposupright.y -= uprightsepdist;
                        else
                            desiredposupright.y += uprightsepdist;
//                        desiredposupright.z -= uprightsepdist;

                        ship->formationOffset = desiredposupright;

                        uprightsepdist = thissepdist;
                        break;

                    case 2:
                        upleftsepdist += thissepdist;
                        if (!prox_sol) upleftsepdist *= DELTA3D_FORMATION_PADDING_SCALE;

                        // upleftback is (1,-1,-1)
                        desiredposupleft.x += upleftsepdist;
                        if(formationtodo->formation.flipState)
                            desiredposupleft.y += upleftsepdist;
                        else
                            desiredposupleft.y -= upleftsepdist;
//                        desiredposupleft.z -= upleftsepdist;

                        ship->formationOffset = desiredposupleft;

                        upleftsepdist = thissepdist;
                        break;

                    case 0:
                        dnleftsepdist += thissepdist;
                        if (!prox_sol) dnleftsepdist *= DELTA3D_FORMATION_PADDING_SCALE;

                        // dnleftback is (-1,-1,-1)
                        desiredposdnleft.x -= dnleftsepdist;
                        if(formationtodo->formation.flipState)
                            desiredposdnleft.y += dnleftsepdist;
                        else
                            desiredposdnleft.y -= dnleftsepdist;
  //                      desiredposdnleft.z -= dnleftsepdist;

                        ship->formationOffset = desiredposdnleft;

                        dnleftsepdist = thissepdist;
                        break;

                    case 3:
                        dnrightsepdist += thissepdist;
                        if (!prox_sol) dnrightsepdist *= DELTA3D_FORMATION_PADDING_SCALE;

                        // dnrightback is (-1,1,-1)
                        desiredposdnright.x -= dnrightsepdist;
                        if(formationtodo->formation.flipState)
                            desiredposdnright.y -= dnrightsepdist;
                        else
                            desiredposdnright.y += dnrightsepdist;
//                        desiredposdnright.z -= dnrightsepdist;

                        ship->formationOffset = desiredposdnright;

                        dnrightsepdist = thissepdist;
                        break;
                }
            }
        }
        break;

        case CLAW_FORMATION:
        {
            // offset in format up,right,heading for x,y,z
            vector desiredposupright = { 0.0f, 0.0f, 0.0f };
            vector desiredposupleft = { 0.0f, 0.0f, 0.0f };
            vector desiredposdnright = { 0.0f, 0.0f, 0.0f };
            vector desiredposdnleft = { 0.0f, 0.0f, 0.0f };
            real32 uprightsepdist;
            real32 upleftsepdist;
            real32 dnrightsepdist;
            real32 dnleftsepdist;
            real32 thissepdist;

            real32 forwardmodifier = 1.0f;
            real32 forsepdist;

            if (prox_sol)
            {
                uprightsepdist = searchRadius;
            }
            else
            {
                uprightsepdist = leaderstatic->common.staticCollInfo.collspheresize*leaderstatic->formationPaddingModifier*tacticsFormationPadding;
            }

            upleftsepdist = uprightsepdist;
            dnrightsepdist = uprightsepdist;
            dnleftsepdist = uprightsepdist;

            for (i=1;i<numShips;i++)
            {
                ship = selection->ShipPtr[i];
                shipstatic = (ShipStaticInfo *)ship->staticinfo;

                if (prox_sol)
                {
                    thissepdist = searchRadius;
                }
                else
                {
                    thissepdist = shipstatic->common.staticCollInfo.collspheresize*shipstatic->formationPaddingModifier*tacticsFormationPadding;
                }

                switch (i & 3)
                {
                    case 1:
                        uprightsepdist += thissepdist;
                        if (!prox_sol) uprightsepdist *= CLAW_FORMATION_PADDING_SCALE;
                        forsepdist = uprightsepdist * forwardmodifier;

                        // uprightforward is (1,1,1)
                        desiredposupright.x += uprightsepdist;
                        if(formationtodo->formation.flipState)
                            desiredposupright.y -= uprightsepdist;
                        else
                            desiredposupright.y += uprightsepdist;
                        desiredposupright.z += forsepdist;

                        ship->formationOffset = desiredposupright;

                        uprightsepdist = thissepdist;
                        break;

                    case 2:
                        upleftsepdist += thissepdist;
                        if (!prox_sol) upleftsepdist *= CLAW_FORMATION_PADDING_SCALE;
                        forsepdist = upleftsepdist * forwardmodifier;

                        // upleftforward is (1,-1,1)

                        desiredposupleft.x += upleftsepdist;
                        if(formationtodo->formation.flipState)
                            desiredposupleft.y += upleftsepdist;
                        else
                            desiredposupleft.y -= upleftsepdist;
                        desiredposupleft.z += forsepdist;

                        ship->formationOffset = desiredposupleft;

                        upleftsepdist = thissepdist;
                        break;

                    case 0:
                        dnleftsepdist += thissepdist;
                        if (!prox_sol) dnleftsepdist *= CLAW_FORMATION_PADDING_SCALE;
                        forsepdist = dnleftsepdist * forwardmodifier;

                        // dnleftforward is (-1,-1,1)
                        desiredposdnleft.x -= dnleftsepdist;
                        if(formationtodo->formation.flipState)
                            desiredposdnleft.y += dnleftsepdist;
                        else
                            desiredposdnleft.y -= dnleftsepdist;
                        desiredposdnleft.z += forsepdist;

                        ship->formationOffset = desiredposdnleft;

                        dnleftsepdist = thissepdist;

                        forwardmodifier *= CLAW_FORMATION_CURVATURE;
                        break;

                    case 3:
                        dnrightsepdist += thissepdist;
                        if (!prox_sol) dnrightsepdist *= CLAW_FORMATION_PADDING_SCALE;
                        forsepdist = dnrightsepdist * forwardmodifier;

                        // dnrightforward is (-1,1,1)
                        desiredposdnright.x -= dnrightsepdist;
                        if(formationtodo->formation.flipState)
                            desiredposdnright.y -= dnrightsepdist;
                        else
                            desiredposdnright.y += dnrightsepdist;
                        desiredposdnright.z += forsepdist;

                        ship->formationOffset = desiredposdnright;

                        dnrightsepdist = thissepdist;
                        break;
                }
            }
        }
        break;

        case WALL_FORMATION:
        {
            sdword squaresize;
            sdword curpos,currow;
            real32 sepdist;
            vector desiredpos = { 0.0f, 0.0f, 0.0f };
            vector desiredv = { 0.0f, 0.0f, 0.0f };
            Ship *ship;
            ShipStaticInfo *shipstatic;

            squaresize = (udword)fsqrt((real32)numShips);
            if ((squaresize*squaresize) < numShips)
            {
                squaresize++;
            }

            if (prox_sol)
            {
                sepdist = searchRadius*2.0f;
            }
            else
            {
                sepdist = leaderstatic->common.staticCollInfo.collspheresize*tacticsFormationPadding*
                            leaderstatic->formationPaddingModifier*WALL_FORMATION_PADDING_SCALE + WALL_FORMATION_PADDING;
            }

            // rightspacing is (0,1,0)
            // downspacing is (-1,0,0)
            if(formationtodo->formation.flipState)
                desiredpos.y -= sepdist;
            else
                desiredpos.y += sepdist;

            for (curpos=1,currow=0,i=1;i<numShips;i++)
            {
                ship = selection->ShipPtr[i];
                shipstatic = (ShipStaticInfo *)ship->staticinfo;

                ship->formationOffset = desiredpos;

                curpos++;
                if (curpos >= squaresize)
                {
                    curpos = 0;
                    currow++;
                    desiredv.x -= sepdist;
                    desiredpos = desiredv;
                }
                else
                {
                    if(formationtodo->formation.flipState)
                        desiredpos.y -= sepdist;
                    else
                        desiredpos.y += sepdist;
                }
            }
        }
        break;

        case CUSTOM_FORMATION:
            {
                vector desiredpos = { 0.0f, 0.0f, 0.0f };
                vector averagepos = { 0.0f, 0.0f, 0.0f };
                vector error      = { 0.0f, 0.0f, 0.0f };
                Ship *ship;
                real32 minError,totalError;
                sdword minErrorIndex;


                //get center point
                for(i=0;i<numShips;i++)
                {
                    vecAddTo(averagepos,selection->ShipPtr[i]->obj.posinfo.position);
                }
                vecScalarMultiply(averagepos,averagepos,1.0f/numShips);

                //find ship closest to centerpoint to use for leader
                minError = REALlyBig;
                minErrorIndex = 0;
                for(i=0;i<numShips;i++)
                {
                    vecSub(error,selection->ShipPtr[i]->obj.posinfo.position,averagepos);
                    error.x = ABS(error.x);
                    error.y = ABS(error.y);
                    error.z = ABS(error.z);
                    totalError = error.x+error.y+error.z;
                    if(totalError < minError)
                    {
                        //new closest ship!
                        minError = totalError;
                        minErrorIndex = i;
                    }
                }

                //swap center ship with leader position
                ship = selection->ShipPtr[0];
                selection->ShipPtr[0] = selection->ShipPtr[minErrorIndex];
                selection->ShipPtr[minErrorIndex] = ship;

                leader = selection->ShipPtr[0];

                //convert all ships current positions to relatavistic offsets
                for(i=1;i<numShips;i++)
                {
                    ship = (Ship *) selection->ShipPtr[i];

                    vecSub(desiredpos,ship->obj.posinfo.position,leader->obj.posinfo.position);
                    matMultiplyVecByMat(&ship->formationOffset,&desiredpos,&leader->rotatable.rotinfo.coordsys);
                }
            }
        break;
        default:
            dbgAssertOrIgnore(FALSE);
            break;
    }
}

real32 calculateTravelDistance(CommandToDo *formationtodo)
{
    sdword i;
    SelectCommand *selection = formationtodo->selection;
    sdword numShips = selection->numShips;
    Ship *leader = selection->ShipPtr[0];
    matrix *coordsys;
    vector position;
    vector desiredposition;
    Ship *ship;
    real32  distanceSqr=0.0f;
    vector distvec;

    position = leader->obj.posinfo.position;
    coordsys = &leader->rotatable.rotinfo.coordsys;

    switch (formationtodo->formation.formationtype)
    {
        case DELTA_FORMATION:
        case BROAD_FORMATION:
        case DELTA3D_FORMATION:
        case CLAW_FORMATION:
        case WALL_FORMATION:
        case CUSTOM_FORMATION:
            for (i=1;i<numShips;i++)
            {
                ship = selection->ShipPtr[i];

                //calculate realspace position
                matMultiplyMatByVec(&desiredposition,coordsys,&ship->formationOffset);
                vecAddTo(desiredposition,position);

                //calculate distance ship must go
                vecSub(distvec,ship->obj.posinfo.position,desiredposition);
                distanceSqr += vecMagnitudeSquared(distvec);
            }
        case SPHERE_FORMATION:
            break;
    }
    return distanceSqr;
}

//if smallest ship radius/biggest ship radius is bigger than this, then
//we ignore leader size in choosing a leader and find the center leader!
#define BIG_SMALL_RATIO_TO_BE_EQUAL_FOR_LEADER      0.7f

void formationArrageCrazyOptimum(CommandToDo *formationcommand)
{
    //algorithm...go through each ship...selecting each to be the leader
    //then go through each iteration of that to calculate the minimum distance
    //then select the minimum distance for actual use!
    sdword i,j,a,p;
    sdword numShips = formationcommand->selection->numShips;
    SelectCommand *selection = formationcommand->selection;
    SelectCommand *optimumSel;
    SelectCommand *originalSel,*testSel;
    real32 tempdist,biggestToSmallestRatio;
    sdword numWhoWant,poswanted;
    real32 minDist = REALlyBig;
    real32 mindistleader,tempdist3;

    vector positionsMax[COMMAND_MAX_SHIPS];
    sdword shipWantsPositionMax[COMMAND_MAX_SHIPS];
    sdword shipGetsPositionMax[COMMAND_MAX_SHIPS];
    sdword positionTakenMax[COMMAND_MAX_SHIPS];

    vector *positions;
    sdword *shipWantsPosition;
    sdword *shipGetsPosition;
    sdword *positionTaken;
    bool32 redo = FALSE;

    matrix *coordsys;
    //vector center = {0.0f,0.0f,0.0f};
    sdword shipwhogetsit = 0;
    vector tempvec;
    Ship *ship;
    //could possible select 'middle' ship as leader...would be faster...
    //will test effect on efficiency

    //allocation enough memory for
    optimumSel = memAlloc(sizeofSelectCommand(selection->numShips),"FormationOpt2",0);
    originalSel = memAlloc(sizeofSelectCommand(selection->numShips),"FormationOpt1",0);
    testSel = memAlloc(sizeofSelectCommand(selection->numShips),"FormationOpt1",0);


    if (numShips > COMMAND_MAX_SHIPS-1)
    {
        positions=memAlloc(sizeof(vector)*numShips,"forma1",0);
        shipWantsPosition=memAlloc(sizeof(sdword)*numShips,"forma2",0);
        shipGetsPosition=memAlloc(sizeof(sdword)*numShips,"forma3",0);
        positionTaken=memAlloc(sizeof(sdword)*numShips,"forma4",0);
    }
    else
    {
        positions=positionsMax;
        shipWantsPosition=shipWantsPositionMax;
        shipGetsPosition=shipGetsPositionMax;
        positionTaken=positionTakenMax;
    }

    optimumSel->numShips = numShips;

    formationArrangeOptimum(formationcommand);
    biggestToSmallestRatio = selection->ShipPtr[numShips-1]->staticinfo->common.staticCollInfo.collspheresize/selection->ShipPtr[0]->staticinfo->common.staticCollInfo.collspheresize;
    if(biggestToSmallestRatio > BIG_SMALL_RATIO_TO_BE_EQUAL_FOR_LEADER)
    {
        redo=TRUE;
/*
        for(i=0;i<numShips;i++)
        {
            vecAddTo(center,selection->ShipPtr[i]->obj.posinfo.position);
        }
        tempdist = 1.0f/numShips;
        vecScalarMultiply(center,center,tempdist);
        minDist = REALlyBig;
        for(i=0;i<numShips;i++)
        {
            vecSub(tempvec,selection->ShipPtr[i]->obj.posinfo.position,center);
            tempdist = vecMagnitudeSquared(tempvec);
            if(tempdist < minDist)
            {
                minDist = tempdist;
                shipwhogetsit = i;
            }
        }

        ship = selection->ShipPtr[0];
        selection->ShipPtr[0] = selection->ShipPtr[shipwhogetsit];
        selection->ShipPtr[shipwhogetsit] = ship;
*/
    }

    //find center ship...i.e. find leader!

    //assume ships are in the correct order of sizeness
    //and the leader has been picked!  I.E. position 0 is secure!

    //go through all leaders and use optimal version IF redo is true

    originalSel->numShips = numShips;
    for(a=0;a<numShips;a++)
    {
        originalSel->ShipPtr[a] = selection->ShipPtr[a];
    }

    mindistleader = REALlyBig;

    for(a=0;a<numShips;a++)
    {
        //reassemble original list
        for(p=0;p<numShips;p++)
        {
            selection->ShipPtr[p] = originalSel->ShipPtr[p];
        }
        //use a as leader
        selection->ShipPtr[0] = selection->ShipPtr[a];
        selection->ShipPtr[a] = originalSel->ShipPtr[0];

        FormationCalculateOffsets(formationcommand);
        coordsys = &selection->ShipPtr[0]->rotatable.rotinfo.coordsys;
        for(i=1;i<numShips;i++)
        {
            ship = selection->ShipPtr[i];
            matMultiplyMatByVec(&positions[i],coordsys,&ship->formationOffset);
            vecAddTo(positions[i],selection->ShipPtr[0]->obj.posinfo.position);

            // This line doesn't do anything but it was probably intended to do something...
            // I've left it here in case someone debugging this code needs this as part of the solution!
            //selection->ShipPtr[i]->formationOffset;

            shipWantsPosition[i]=0;
            shipGetsPosition[i]=0;
            positionTaken[i] = FALSE;
        }

        //positions,posiitiontaken,shipwantsposiiton initialized
        //find position that ship WANTS
        for(i=1;i<numShips;i++)
        {
            ship = selection->ShipPtr[i];
            minDist = REALlyBig;
            for(j=1;j<numShips;j++)
            {
                vecSub(tempvec,ship->obj.posinfo.position,positions[j]);
                tempdist = vecMagnitudeSquared(tempvec);
                if(tempdist < minDist)
                {
                    //new best point
                    minDist = tempdist;
                    shipWantsPosition[i] = j;
                }
            }
        }

        //shipWantsPosition full of points that each ship WANTS to go to
        //now go through list and assign positions in
        for(i=1;i<numShips;i++)
        {
            poswanted = shipWantsPosition[i];
            if(poswanted == 0)
                continue;
            numWhoWant = 1;
            //see if someone else wants this position too...
            for(j=1;j<numShips;j++)
            {
                if(j==i)
                    continue;
                if(shipWantsPosition[j] == poswanted)
                {
                    //contention exists for point poswanted
                    numWhoWant++;
                }
            }
            if(numWhoWant == 1)
            {
                //no contention
                shipGetsPosition[i] = poswanted;
                positionTaken[poswanted] = TRUE;
            }
            else
            {
                //numWhoWant > 1 hence contention
                //must resolve conflict
                minDist = REALlyBig;
                for(j=1;j<numShips;j++)
                {
                    ship = selection->ShipPtr[j];
                    if(shipWantsPosition[j] == poswanted)
                    {
                        vecSub(tempvec,ship->obj.posinfo.position,positions[poswanted]);
                        tempdist = vecMagnitudeSquared(tempvec);
                        if(tempdist < minDist)
                        {
                            minDist = tempdist;
                            shipwhogetsit = j;
                        }
                    }
                }
                //shipwhogetsit is the shipindex of the ship that won the point
                for(j=1;j<numShips;j++)
                {
                    if(shipWantsPosition[j] == poswanted &&
                       j != shipwhogetsit)
                    {
                        shipWantsPosition[j] = 0;   //set so we know to fix it up later
                        shipGetsPosition[j] = 0;
                    }
                }
                shipGetsPosition[shipwhogetsit] = poswanted;
                positionTaken[poswanted] = TRUE;
            }
        }
        //now match up free ships with closest free slots
        for(i=1;i<numShips;i++)
        {
            if(shipWantsPosition[i] == 0)
            {
                //ship needs a position
                minDist = REALlyBig;
                ship = selection->ShipPtr[i];
                for(j=1;j<numShips;j++)
                {
                    if(positionTaken[j] == FALSE)
                    {
                        vecSub(tempvec,ship->obj.posinfo.position,positions[j]);
                        tempdist = vecMagnitudeSquared(tempvec);
                        if(tempdist < minDist)
                        {
                            minDist = tempdist;
                            shipwhogetsit = j;      //use as tempvar
                        }
                    }
                }
                shipWantsPosition[i] = shipwhogetsit;   //set so we know to fix it up later
                shipGetsPosition[i] = shipwhogetsit;
                positionTaken[shipwhogetsit] = TRUE;
            }
        }

        testSel->ShipPtr[0] = selection->ShipPtr[0];
        for(i=1;i<numShips;i++)
        {
        #ifdef HW_BUILD_FOR_DEBUGGING
            dbgAssertOrIgnore(shipGetsPosition[i] != 0);
        #endif
            testSel->ShipPtr[shipGetsPosition[i]] = selection->ShipPtr[i];
        }
        for(i=0;i<numShips;i++)
        {
            selection->ShipPtr[i]=testSel->ShipPtr[i];
        }
        //selection has been re-organized correctly...so now lets
        //recalculate the proper offsets and start drinking
        FormationCalculateOffsets(formationcommand);

        tempdist3 = calculateTravelDistance(formationcommand);
        if(tempdist3 < mindistleader)
        {
            mindistleader = tempdist3;
            for(i=0;i<numShips;i++)
            {
                optimumSel->ShipPtr[i] = selection->ShipPtr[i];
            }
        }

        if (!redo)
        {
            break;
        }
    }

/*    optimumSel->ShipPtr[0] = selection->ShipPtr[0];
    for(i=1;i<numShips;i++)
    {
#ifdef HW_BUILD_FOR_DEBUGGING
        dbgAssertOrIgnore(shipGetsPosition[i] != 0);
#endif
        optimumSel->ShipPtr[shipGetsPosition[i]] = selection->ShipPtr[i];
    }

  */
    for(i=0;i<numShips;i++)
    {
        selection->ShipPtr[i]=optimumSel->ShipPtr[i];
    }
    //selection has been re-organized correctly...so now lets
    //recalculate the proper offsets and start drinking
    FormationCalculateOffsets(formationcommand);

    //MAYBE make the big/small arangement here...then do this?

    memFree(optimumSel);
    memFree(testSel);
    memFree(originalSel);

    if (numShips > COMMAND_MAX_SHIPS-1)
    {
        memFree(positions);
        memFree(shipWantsPosition);
        memFree(shipGetsPosition);
        memFree(positionTaken);
    }

}

/*-----------------------------------------------------------------------------
    Name        : FillInFormationSpecifics
    Description : fills in the formation details
    Inputs      : formationcommand
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: FillInFormationSpecifics 0x00484950 takes the force-field level (FormationCommand.catForceFieldLevel, +0x288);
//      no FillInShipFormationStuff (Cataclysm has no Ship.formationcommand); the force-field formations also start
//      without specific info, and any other type is left alone (no assertion)
void FillInFormationSpecifics(CommandToDo *formationcommand,TypeOfFormation formation,sdword catForceFieldLevel,sdword newformation)
{
    // fill in formation specifics:
    formationcommand->formation.catForceFieldLevel = catForceFieldLevel;
    formationcommand->formation.formationtype = formation;
    formationcommand->formation.tacticalState = TightFormation;       //default behavior
    formationcommand->formation.formationLocked = FALSE;                //initialize a formation as unlocked
    formationcommand->formation.tacticsUpdate = 0.0f;                 //this formation needs updating
    formationcommand->formation.flipselectionfixed = FALSE;
    formationcommand->formation.needFix = FALSE;
    formationcommand->formation.enders = FALSE;
    formationcommand->formation.percentmaxspeed = FORMATION_TRAVELVEL_MINSCALE;
    setFormationTravelVelocity(formationcommand);
    formationcommand->formation.flagTravelSlowestShip = FORMATION_TRAVEL_SLOWEST_SHIP;  // obsolete flag, but set anyway
    formationcommand->formation.doneInitialAttack = FALSE;
    formationcommand->formation.sortorder = 0;
    formationcommand->formation.pulse = 0;

    if(newformation)
    {
        //formation group is new so set this properly
        formationcommand->formation.flipState = 0;
    }

    formationArrageCrazyOptimum(formationcommand);

    switch (formation)
    {
        case DELTA_FORMATION:
        case BROAD_FORMATION:
        case DELTA3D_FORMATION:
        case CLAW_FORMATION:
        case WALL_FORMATION:
        case SPHERE_FORMATION:
        case CUSTOM_FORMATION:
        case FORCEFIELD_WALL_FORMATION:
        case FORCEFIELD_SPHERE_FORMATION:
            formationcommand->formation.formationSpecificInfo = NULL;
            break;

        default:
            break;
    }
}

typedef struct UniqueObjAndFreq
{
    void *obj;
    sdword freq;
} UniqueObjAndFreq;

typedef struct CountFreqObjs
{
    sdword numUniqueObjs;
    UniqueObjAndFreq *uniqueObjFreqs;
} CountFreqObjs;

void uofInit(CountFreqObjs *countFreqObjs,sdword max)
{
    countFreqObjs->numUniqueObjs = 0;
    countFreqObjs->uniqueObjFreqs = memAlloc(sizeof(UniqueObjAndFreq)*max,"uniqueobjfreq",Pyrophoric);
}

void uofClose(CountFreqObjs *countFreqObjs)
{
    memFree(countFreqObjs->uniqueObjFreqs);
}

void uofAddObj(CountFreqObjs *countFreqObjs,void *obj)
{
    sdword i;

    for (i=0;i<countFreqObjs->numUniqueObjs;i++)
    {
        if (obj == countFreqObjs->uniqueObjFreqs[i].obj)
        {
             countFreqObjs->uniqueObjFreqs[i].freq++;
             return;
        }
    }

    countFreqObjs->uniqueObjFreqs[countFreqObjs->numUniqueObjs].obj = obj;
    countFreqObjs->uniqueObjFreqs[countFreqObjs->numUniqueObjs].freq = 1;
    countFreqObjs->numUniqueObjs++;
}

void *uofFindObjWithMaxFreq(CountFreqObjs *countFreqObjs)
{
    sdword i;
    sdword max = -1;
    sdword maxindex = -1;

    for (i=0;i<countFreqObjs->numUniqueObjs;i++)
    {
        if (countFreqObjs->uniqueObjFreqs[i].freq > max)
        {
            maxindex = i;
            max = countFreqObjs->uniqueObjFreqs[i].freq;
        }
    }

    if (maxindex != -1)
    {
        return countFreqObjs->uniqueObjFreqs[maxindex].obj;
    }

    return NULL;
}


// CAT: MajorityShipsAreDoing 0x00484a30 returns the majority's move as a path: the most frequent move command's
//      path type, point count and current point, and its points from the current one on. The exe copies point
//      curPoint + k for k < numPoints and wraps only when curPoint >= numPoints, so past the path's end it reads the
//      command's next words (inside the CommandToDo); this port reads them the same way, as bytes.
sdword MajorityShipsAreDoing(SelectCommand *selectcom,sdword *pathType,sdword *numPoints,vector *points,sdword *curPoint,GrowSelection *attacking)
{
    sdword numShips = selectcom->numShips;
    sdword i;
    Ship *ship;
    sdword nummoving = 0;
    sdword numattacking = 0;

    // first pass: are any ships moving or attacking?
    for (i=0;i<numShips;i++)
    {
        ship = selectcom->ShipPtr[i];
        if (ship->command)
        {
            if (ship->command->ordertype.order == COMMAND_ATTACK)
            {
                numattacking++;
            }
            else if (ship->command->ordertype.order == COMMAND_MOVE)
            {
                nummoving++;
            }
        }
    }

    if (numattacking && nummoving)
    {
        return 0;       // ambiguous - some attacking, some moving
    }

    if (numattacking > (numShips - numattacking))
    {
        if (AreAllShipsAttackCapable(selectcom))
        {
            // amalgamate a list of targets everyone is going to attack in formation
            growSelectInit(attacking);

            for (i=0;i<numShips;i++)
            {
                ship = selectcom->ShipPtr[i];
                if ((ship->command) && (ship->command->ordertype.order == COMMAND_ATTACK))
                {
                    SelectCommand *attacksel = (SelectCommand *)ship->command->attack;
                    sdword numTargets = attacksel->numShips;
                    sdword j;
                    for (j=0;j<numTargets;j++)
                    {
                        if (!ShipInSelection(selectcom,attacksel->ShipPtr[j]))      // in case force attacking
                        {
                            growSelectAddShipNoDuplication(attacking,attacksel->ShipPtr[j]);
                        }
                    }
                }
            }

            if (attacking->selection->numShips == 0)
            {
                growSelectClose(attacking);
                return 0;
            }

            return COMMAND_ATTACK;
        }
        else
        {
            return 0;       // not all ships attack capable - therefore do not attack
        }
    }
    else if (nummoving > (numShips - nummoving))
    {
        if (AreShipsMobile(selectcom))
        {
            // find a tovector the majority of ships are moving to
            CountFreqObjs countFreqObjs;
            CommandToDo *mostfreqmoveto;
            sdword k;

            uofInit(&countFreqObjs,nummoving);

            for (i=0;i<numShips;i++)
            {
                ship = selectcom->ShipPtr[i];
                if ((ship->command) && (ship->command->ordertype.order == COMMAND_MOVE))
                {
                    uofAddObj(&countFreqObjs,ship->command);
                }
            }

            mostfreqmoveto = uofFindObjWithMaxFreq(&countFreqObjs);
            uofClose(&countFreqObjs);

            if (!mostfreqmoveto)
            {
                return 0;
            }

            *pathType = mostfreqmoveto->move.pathType;
            *numPoints = mostfreqmoveto->move.numPoints;
            *curPoint = mostfreqmoveto->move.curPoint;
            for (k = 0; k < mostfreqmoveto->move.numPoints; k++)
            {
                sdword cur = mostfreqmoveto->move.curPoint;
                sdword index = cur + k;
                sdword prev = (cur - 1 < 0) ? mostfreqmoveto->move.numPoints : cur - 1;

                if (mostfreqmoveto->move.numPoints <= cur)
                {
                    index -= mostfreqmoveto->move.numPoints;
                }
                if (index != prev)
                {
                    memcpy(&points[k],(ubyte *)&mostfreqmoveto->move.points[0] + index * sizeof(vector),sizeof(vector));
                }
            }

            return COMMAND_MOVE;
        }
        else
        {
            return 0;       // not all ships are mobile - therefore do not move
        }
    }
    else
    {
        return 0;           // no one attacking or moving
    }
}

/*-----------------------------------------------------------------------------
    Name        : clFormation
    Description : puts the ships of selectcom into a formation
    Inputs      : comlayer, selectcom, formation, catForceFieldLevel (the force-field formations' level)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: clFormation 0x00484c80: the fourth argument is the force-field level; each ship's header can refuse the
//      formation (slot 34 CustShipFormationCommand(ship, formation, level)); a force-field sphere takes at most 12
//      ships and a force-field wall 24 (the others leave the local selection); a formation of one ship is allowed;
//      a change to a force-field formation stops passive attacking; the same type at the same level does nothing; a
//      new formation of a majority on the move takes over their path.
void clFormation(CommandLayer *comlayer,SelectCommand *selectcom,TypeOfFormation formation,sdword catForceFieldLevel)
{
    CommandToDo *newcommand;
    CommandToDo *alreadyformation;
    SelectCommand *selection;
    udword sizeofselection;
    sdword majorityshipsare;
    sdword pathType = 0, numPoints = 0, curPoint = 0;
    vector points[CAT_MOVE_MAX_POINTS];   // CAT-FIX: zeroed (the exe's stack copy keeps what it held where MajorityShipsAreDoing skips a point)
    GrowSelection majorityshipsattacking;
    MaxSelection filtered;
    sdword i;
    sdword cap = 0;

    memset(points,0,sizeof(points));

    switch (formation)
    {
        case DELTA_FORMATION:
        case BROAD_FORMATION:
        case DELTA3D_FORMATION:
        case CLAW_FORMATION:
        case WALL_FORMATION:
        case SPHERE_FORMATION:
        case CUSTOM_FORMATION:
        case FORCEFIELD_WALL_FORMATION:
        case FORCEFIELD_SPHERE_FORMATION:
            break;
        default:
            dbgFatalf(DBG_Loc,"Invalid formation %d",formation);
    }

    filtered.numShips = 0;
    for (i = 0; i < selectcom->numShips; i++)
    {
        Ship *ship = selectcom->ShipPtr[i];
        // the exe passes the level as a third argument; the only implementer, SentinelFormationCommand 0x005c6df0,
        // reads ship and formation
        if ((ship->staticinfo->custshipheader.CustShipFormationCommand == NULL) ||
            ship->staticinfo->custshipheader.CustShipFormationCommand(ship,(sdword)formation))
        {
            filtered.ShipPtr[filtered.numShips++] = ship;
        }
    }
    if (filtered.numShips < 1)
    {
        return;
    }
    if ((formation == FORCEFIELD_SPHERE_FORMATION) && (filtered.numShips > MAX_SENTINELS_IN_FORCE_FIELD_SPHERE))
    {
        cap = MAX_SENTINELS_IN_FORCE_FIELD_SPHERE;
    }
    else if ((formation == FORCEFIELD_WALL_FORMATION) && (filtered.numShips > 24))
    {
        cap = 24;
    }
    if (cap != 0)
    {
        for (i = cap; i < filtered.numShips; i++)
        {
            clRemoveShipFromSelection(&selSelected,filtered.ShipPtr[i]);
            filtered.ShipPtr[i] = NULL;
        }
        filtered.numShips = cap;
    }
    selectcom = (SelectCommand *)&filtered;

    if ((alreadyformation = IsSelectionAlreadyDoingSomething(comlayer,selectcom)) != NULL)
    {
        if (alreadyformation->ordertype.attributes & COMMAND_MASK_FORMATION)
        {
            if (((formation == FORCEFIELD_SPHERE_FORMATION) || (formation == FORCEFIELD_WALL_FORMATION)) &&
                (alreadyformation->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING))
            {
                ClearPassiveAttacking(alreadyformation);
            }
            if ((alreadyformation->formation.formationtype == formation) &&
                (alreadyformation->formation.catForceFieldLevel == catForceFieldLevel))
            {
                return;
            }
            if (alreadyformation->formation.formationSpecificInfo != NULL)
            {
                memFree(alreadyformation->formation.formationSpecificInfo);
                alreadyformation->formation.formationSpecificInfo = NULL;
            }

            alreadyformation->formation.catForceFieldLevel = catForceFieldLevel;
            alreadyformation->formation.formationtype = formation;
            // HW1's formationTypeHasChanged, inline: re-sort when the sort type changes
            if ((formationSortType[formation] != alreadyformation->formation.sortorder) &&
                (alreadyformation->selection->numShips != 1) && (formationSortType[formation] == FORMATION_SORT_BIGGEST_THEN_CLOSEST))
            {
                formationBiggestThenClosestOptimum(alreadyformation->selection,formation);
                alreadyformation->formation.sortorder = FORMATION_SORT_BIGGEST_THEN_CLOSEST;
            }
            FormationCalculateOffsets(alreadyformation);

            PrepareShipsForCommand(alreadyformation,TRUE);
            return;
        }
        else if ((alreadyformation->ordertype.order == COMMAND_MOVE) ||
                 (alreadyformation->ordertype.attributes & COMMAND_MASK_PROTECTING) ||
                 (alreadyformation->ordertype.order == COMMAND_ATTACK))
        {
            // Change from normal move, protect or attack command to the same in formation
            alreadyformation->ordertype.attributes |= COMMAND_MASK_FORMATION;
            FillInFormationSpecifics(alreadyformation,formation,catForceFieldLevel,TRUE);
            PrepareShipsForCommand(alreadyformation,TRUE);
            return;
        }
    }

    majorityshipsare = MajorityShipsAreDoing(selectcom,&pathType,&numPoints,points,&curPoint,&majorityshipsattacking);

    RemoveShipsFromDoingStuff(comlayer,selectcom,0x20);

    newcommand = memAlloc(sizeof(CommandToDo),"ToDo",NonVolatile);
    InitCommandToDo(newcommand);

    sizeofselection = sizeofSelectCommand(selectcom->numShips);
    selection = memAlloc(sizeofselection,"ToDoSelection",NonVolatile);
    memcpy(selection,selectcom,sizeofselection);

    newcommand->selection = selection;
    newcommand->ordertype.order = COMMAND_NULL;
    newcommand->ordertype.attributes = COMMAND_MASK_FORMATION;

    FillInFormationSpecifics(newcommand,formation,catForceFieldLevel,TRUE);

    InitShipsForAI(selection,TRUE);

    PrepareShipsForCommand(newcommand,TRUE);
    listAddNode(&comlayer->todolist,&newcommand->todonode,newcommand);

    if (majorityshipsare == COMMAND_MOVE)
    {
        // the path from the point before the current one (0x00485002: the copied list's [curPoint - 1], or its last)
        sdword from = (curPoint - 1 < 0) ? numPoints - 1 : curPoint - 1;
        ChangeOrderToMove(newcommand,pathType,points[from],numPoints,points);
    }
    else if (majorityshipsare == COMMAND_ATTACK)
    {
        ChangeOrderToAttack(newcommand,(SelectAnyCommand *)majorityshipsattacking.selection,4);   // CAT: reason 4 (clFormation 0x00485058)
        growSelectClose(&majorityshipsattacking);
    }
}

/*=============================================================================
    The following routines are for the military parades ships form when they are launched from a ship
=============================================================================*/

typedef struct
{
    real32 upoffset;
    real32 rightoffset;
    real32 headingoffset;
    real32 upspacing;
    real32 rightspacing;
    real32 headingspacing;
    udword facedirection;
} SlotInfo;

typedef struct
{
    SlotInfo slotinfos[MAX_MILITARY_SLOTS];
} ParadeTypeInfo;

// CAT: 12 parade types, one per Cataclysm parade_*.script, in the exe's array order (paradeTypeInfos 0x00a3e448,
//      12 x 0x6E4 bytes, filled by paradeSetTweakables 0x004850d0). Each ParadeTypeInfo holds the 63 Cataclysm slots
//      (Formation.h SLOT_SRECON .. SLOT_Misc, 63 x 0x1C = 0x6E4).
_Static_assert(sizeof(SlotInfo) == 0x1C, "SlotInfo is 0x1C bytes in cataclysm.exe");
_Static_assert(sizeof(ParadeTypeInfo) == 0x6E4, "ParadeTypeInfo is 0x6E4 bytes in cataclysm.exe (12 at 0x00a3e448)");
#define PARADE_SMOTHERSHIP      0
#define PARADE_BMOTHERSHIP      1
#define PARADE_SCARRIER         2
#define PARADE_BCARRIER         3
#define PARADE_SLIPGATE         4
#define PARADE_SPROCESSOR       5
#define PARADE_BPROCESSOR       6
#define PARADE_DPRISONSHIP      7
#define PARADE_R1CARRIER        8
#define PARADE_R2CARRIER        9
#define PARADE_P1MOTHERSHIP     10
#define PARADE_BHEAVYCRUISER    11
#define NUMBER_PARADE_TYPES     12

#define SUBSLOT_BATCH      10      // subslots are allocated in batches of this many

static ParadeTypeInfo paradeTypeInfos[NUMBER_PARADE_TYPES];

void scriptSetSlotInfoCB(char *directory,char *field,void *dataToFillIn);

scriptStructEntry ParadeInfoScriptTable[] =
{
    // CAT: the exe's 63 keys in its order (ParadeInfoScriptTable 0x0087fb68, all scriptSetSlotInfoCB 0x00485090)
    { "SlotPositionInfo[SLOT_SRECON]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SRECON], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SMIMIC]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SMIMIC], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SMCV]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SMCV], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SWORKER]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SWORKER], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SPROCESSOR]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SPROCESSOR], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SCARRIER0]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SCARRIER0], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SCARRIER1]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SCARRIER1], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SCARRIER2]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SCARRIER2], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SSENTINEL]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SSENTINEL], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SACOLYTE]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SACOLYTE], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SACV]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SACV], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SRAMMINGFRIGATE]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SRAMMINGFRIGATE], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SMULTIBEAMFRIGATE]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SMULTIBEAMFRIGATE], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SHIVEFRIGATE]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SHIVEFRIGATE], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SDREADNOUGHT]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SDREADNOUGHT], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SLEECH]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SLEECH], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SDESTROYER]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SDESTROYER], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SMOTHERSHIP]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SMOTHERSHIP], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BRECON]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BRECON], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BWORKER]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BWORKER], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BPROCESSOR]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BPROCESSOR], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BHIVEFRIGATE]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BHIVEFRIGATE], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BMULTIBEAMFRIGATE]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BMULTIBEAMFRIGATE], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BCARRIER0]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BCARRIER0], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BCARRIER1]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BCARRIER1], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BCARRIER2]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BCARRIER2], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BSENTINEL]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BSENTINEL], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BACOLYTE]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BACOLYTE], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BACV]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BACV], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BINTERCEPTOR]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BINTERCEPTOR], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BATTACKBOMBER]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BATTACKBOMBER], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BCLOAKEDFIGHTER]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BCLOAKEDFIGHTER], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BMULTIGUNCORVETTE]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BMULTIGUNCORVETTE], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BDFGFRIGATE]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BDFGFRIGATE], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BHEAVYCORVETTE]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BHEAVYCORVETTE], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BIONARRAYFRIGATE]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BIONARRAYFRIGATE], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BHEAVYCRUISER]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BHEAVYCRUISER], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BMISSILECORVETTE]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BMISSILECORVETTE], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BCRUISEMISSILE]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BCRUISEMISSILE], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BRAMMINGFRIGATE]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BRAMMINGFRIGATE], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_BMOTHERSHIP]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_BMOTHERSHIP], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_Fighter0]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_Fighter0], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_Fighter1]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_Fighter1], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_Fighter2]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_Fighter2], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_Fighter3]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_Fighter3], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_Fighter4]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_Fighter4], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_Fighter5]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_Fighter5], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_Corvette0]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_Corvette0], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_Corvette1]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_Corvette1], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_Corvette2]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_Corvette2], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_Corvette3]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_Corvette3], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_Frigate]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_Frigate], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_ResCollector]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_ResCollector], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_Destroyer]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_Destroyer], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_HeavyCruiser]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_HeavyCruiser], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_NonCombat]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_NonCombat], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_Carrier]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_Carrier], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_SupportCorvette]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_SupportCorvette], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_P1Fighter]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_P1Fighter], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_P1IonArrayFrigate]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_P1IonArrayFrigate], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_P1MissileCorvette]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_P1MissileCorvette], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_P1StandardCorvette]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_P1StandardCorvette], &paradeTypeInfos[0] },
    { "SlotPositionInfo[SLOT_Misc]", scriptSetSlotInfoCB, &paradeTypeInfos[0].slotinfos[SLOT_Misc], &paradeTypeInfos[0] },

    END_SCRIPT_STRUCT_ENTRY
};

void scriptSetSlotInfoCB(char *directory,char *field,void *dataToFillIn)
{
    SlotInfo *slotInfo = (SlotInfo *)dataToFillIn;

    RemoveCommasFromString(field);

    slotInfo->facedirection = 0;
    sscanf(field,"%f %f %f %f %f %f %d",&slotInfo->upoffset,&slotInfo->rightoffset,&slotInfo->headingoffset,&slotInfo->upspacing,&slotInfo->rightspacing,&slotInfo->headingspacing,&slotInfo->facedirection);
}

void paradeSetTweakables(void)
{
    // CAT: Cataclysm's 12 per-ship parade files, in the exe's order (paradeSetTweakables 0x004850d0); HW1's
    //      Parade_R1Mothership/R2Mothership/Carrier/P2Mothership.script are gone
    memset(&paradeTypeInfos[PARADE_SMOTHERSHIP],0,sizeof(ParadeTypeInfo));
    memset(&paradeTypeInfos[PARADE_BMOTHERSHIP],0,sizeof(ParadeTypeInfo));
    memset(&paradeTypeInfos[PARADE_SCARRIER],0,sizeof(ParadeTypeInfo));
    memset(&paradeTypeInfos[PARADE_BCARRIER],0,sizeof(ParadeTypeInfo));
    memset(&paradeTypeInfos[PARADE_SLIPGATE],0,sizeof(ParadeTypeInfo));
    memset(&paradeTypeInfos[PARADE_SPROCESSOR],0,sizeof(ParadeTypeInfo));
    memset(&paradeTypeInfos[PARADE_BPROCESSOR],0,sizeof(ParadeTypeInfo));
    memset(&paradeTypeInfos[PARADE_DPRISONSHIP],0,sizeof(ParadeTypeInfo));
    memset(&paradeTypeInfos[PARADE_BHEAVYCRUISER],0,sizeof(ParadeTypeInfo));
    scriptSetStruct(NULL,"Parade_sMothership.script",ParadeInfoScriptTable,(ubyte *)&paradeTypeInfos[PARADE_SMOTHERSHIP]);
    scriptSetStruct(NULL,"Parade_bMothership.script",ParadeInfoScriptTable,(ubyte *)&paradeTypeInfos[PARADE_BMOTHERSHIP]);
    scriptSetStruct(NULL,"Parade_sCarrier.script",ParadeInfoScriptTable,(ubyte *)&paradeTypeInfos[PARADE_SCARRIER]);
    scriptSetStruct(NULL,"Parade_bCarrier.script",ParadeInfoScriptTable,(ubyte *)&paradeTypeInfos[PARADE_BCARRIER]);
    scriptSetStruct(NULL,"Parade_Slipgate.script",ParadeInfoScriptTable,(ubyte *)&paradeTypeInfos[PARADE_SLIPGATE]);
    scriptSetStruct(NULL,"Parade_sProcessor.script",ParadeInfoScriptTable,(ubyte *)&paradeTypeInfos[PARADE_SPROCESSOR]);
    scriptSetStruct(NULL,"Parade_bProcessor.script",ParadeInfoScriptTable,(ubyte *)&paradeTypeInfos[PARADE_BPROCESSOR]);
    scriptSetStruct(NULL,"Parade_dPrisonShip.script",ParadeInfoScriptTable,(ubyte *)&paradeTypeInfos[PARADE_DPRISONSHIP]);
    memset(&paradeTypeInfos[PARADE_R1CARRIER],0,sizeof(ParadeTypeInfo));
    memset(&paradeTypeInfos[PARADE_R2CARRIER],0,sizeof(ParadeTypeInfo));
    memset(&paradeTypeInfos[PARADE_P1MOTHERSHIP],0,sizeof(ParadeTypeInfo));
    scriptSetStruct(NULL,"Parade_R1Carrier.script",ParadeInfoScriptTable,(ubyte *)&paradeTypeInfos[PARADE_R1CARRIER]);
    scriptSetStruct(NULL,"Parade_R2Carrier.script",ParadeInfoScriptTable,(ubyte *)&paradeTypeInfos[PARADE_R2CARRIER]);
    scriptSetStruct(NULL,"Parade_P1Mothership.script",ParadeInfoScriptTable,(ubyte *)&paradeTypeInfos[PARADE_P1MOTHERSHIP]);
    scriptSetStruct(NULL,"Parade_bHeavyCruiser.script",ParadeInfoScriptTable,(ubyte *)&paradeTypeInfos[PARADE_BHEAVYCRUISER]);
}

// CAT: a parade slot's first position and its step, summed in the exe's order (setMilitaryParade 0x00485790,
//      processMilitaryParadeToDo 0x00485280): offsets heading, right, up, then the position; steps heading, up, right
static void catParadeSlotPosition(vector *positionat,vector *positionchange,vector *position,vector *up,vector *right,vector *heading,SlotInfo *slotinfo)
{
    positionat->x = heading->x * slotinfo->headingoffset + right->x * slotinfo->rightoffset + up->x * slotinfo->upoffset + position->x;
    positionat->y = heading->y * slotinfo->headingoffset + right->y * slotinfo->rightoffset + up->y * slotinfo->upoffset + position->y;
    positionat->z = heading->z * slotinfo->headingoffset + right->z * slotinfo->rightoffset + up->z * slotinfo->upoffset + position->z;
    positionchange->x = heading->x * slotinfo->headingspacing + up->x * slotinfo->upspacing + right->x * slotinfo->rightspacing;
    positionchange->y = heading->y * slotinfo->headingspacing + up->y * slotinfo->upspacing + right->y * slotinfo->rightspacing;
    positionchange->z = heading->z * slotinfo->headingspacing + up->z * slotinfo->upspacing + right->z * slotinfo->rightspacing;
}

/*-----------------------------------------------------------------------------
    Name        : processMilitaryParadeToDo
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void processMilitaryParadeToDo(struct CommandToDo *command,bool32 passiveAttacked)
{
    MilitaryParadeCommand *militaryParade = command->militaryParade;
    Ship *leader = militaryParade->aroundShip;
    sdword paradeType = militaryParade->paradeType;
    ParadeTypeInfo *paradeTypeInfo = &paradeTypeInfos[paradeType];
    vector up,right,heading;
    vector faceheading;
    sdword slot;
    sdword subslot;
    MilitarySlot *thisslot;
    SlotInfo *thisslotinfo;
    vector positionat;
    vector positionchange;
    Ship *ship;
    ShipStaticInfo *shipstatic;
    bool32 dontrotate;
    bool32 firstShipOfSlot;
    bool32 steadyFormation = TRUE;

    dbgAssertOrIgnore(paradeType >= 0);
    dbgAssertOrIgnore(paradeType < NUMBER_PARADE_TYPES);

    matGetVectFromMatrixCol1(up,leader->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol2(right,leader->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol3(heading,leader->rotatable.rotinfo.coordsys);

    if (vecMagnitudeSquared(leader->obj.posinfo.velocity) >= PARADE_CONSIDERED_STEADY_VELSQR)
    {
        steadyFormation = FALSE;
    }

    // for each slot, position the ship correctly

    for (slot=0;slot<MAX_MILITARY_SLOTS;slot++)
    {
        thisslot = militaryParade->militarySlots[slot];

        if (thisslot->highestSlotUsed < 0)
        {
            continue;
        }

        thisslotinfo = &paradeTypeInfo->slotinfos[slot];

        // calculate starting positonat of this slot
        // CAT: in the exe's order of the sums (heading + right + up + position; heading + up + right), as
        //      setMilitaryParade 0x00485790 and processMilitaryParadeToDo 0x00485280 compute them
        catParadeSlotPosition(&positionat,&positionchange,&leader->obj.posinfo.position,&up,&right,&heading,thisslotinfo);

        switch (thisslotinfo->facedirection)
        {
            case 0:
                faceheading = heading;
                break;
            case 1:
                vecCopyAndNegate(faceheading,heading);
                break;
            case 2:
                faceheading = right;
                break;
            case 3:
                vecCopyAndNegate(faceheading,right);
                break;
            default:
                dbgAssertOrIgnore(FALSE);
                break;
        }

        dbgAssertOrIgnore(thisslot->highestSlotUsed < thisslot->numSubslots);

        firstShipOfSlot = TRUE;

        for (subslot=0;subslot<=thisslot->highestSlotUsed;subslot++)
        {
            ship = thisslot->subslots[subslot].ship;
            if (ship != NULL)
            {
                shipstatic = (ShipStaticInfo *)ship->staticinfo;
                dontrotate = (passiveAttacked & (bool32)shipstatic->rotateToRetaliate);
                // CAT: no HW1 ResearchShip parade case (M2-R4 ships-b)
                if (firstShipOfSlot)
                {
                    firstShipOfSlot = FALSE;
                }
                else
                {
                    ship->shipidle = FALSE;       // only set non-first-ship-of-slot's shipidle to FALSE
                }

                if (dontrotate)
                {
                    if (steadyFormation && MoveReachedDestinationVariable(ship,&positionat,100.0f))
                    {
                        ship->header.flags |= SOF_DontDrawTrails;
                    }
                    if (!MoveReachedDestinationVariable(ship,&positionat,10.0f))
                    {
                        // CAT: no ResearchShip "move closer" parade step, Cataclysm has no research ship type
                        //      (processMilitaryParadeToDo 0x00485280 tests no ship type)
                        aishipFlyToPointAvoidingObjsWithVel(ship,&positionat,AISHIP_FastAsPossible,0.0f,&leader->obj.posinfo.velocity);
                    }
                    else if (steadyFormation)
                    {
                        aitrackSteadyShipDriftOnly(ship);
                    }
                }
                else
                {
                    real32 tmpdistaway = MoveLeftToGo(ship,&positionat);
                    udword tmpuseflags = AISHIP_PointInDirectionFlying | AISHIP_FastAsPossible;
                    bool32 trackheading = FALSE;

                    if (thisslotinfo->facedirection)
                    {
                        trackheading = TRUE;        // if facing nonstandard direction, always track heading
                    }

                    if (isCapitalShipStaticOrBig(shipstatic))
                    {
                        if (tmpdistaway <= 500.0f)
                        {
                            if ((tmpdistaway <= 100.0f) && (steadyFormation)) ship->header.flags |= SOF_DontDrawTrails;
                            trackheading = TRUE;
                        }
                    }
                    else if (tmpdistaway <= 100.0f)
                    {
                        if (steadyFormation)
                        {
                            ship->header.flags |= SOF_DontDrawTrails;
                            // CAT: no ResearchShip "keep heading" case (the type is gone; processMilitaryParadeToDo 0x00485280)
                        }

                        trackheading = TRUE;
                    }

                    if (trackheading)
                    {
                        aitrackHeadingWithBank(ship,&(faceheading),FLYSHIP_HEADINGACCURACY,shipstatic->shipOrDerelict.sinbank);
                        tmpuseflags = AISHIP_FastAsPossible;
                    }

                    if (tmpdistaway >= 10.0f)
                    {
                        // CAT: no ResearchShip "move closer" parade step (the type is gone; processMilitaryParadeToDo 0x00485280)
                        aishipFlyToPointAvoidingObjsWithVel(ship,&positionat,tmpuseflags,0.0f,&leader->obj.posinfo.velocity);
                    }
                    else if (steadyFormation)
                    {
                        aitrackSteadyShipDriftOnly(ship);
                    }
                }
            }

            vecAddTo(positionat,positionchange);
        }
    }
}

bool32 shipInMilitaryParade(ShipPtr ship)
{
    CommandToDo *command = getShipAndItsCommand(&universe.mainCommandLayer,ship);
    if (command == NULL)
    {
        return FALSE;
    }
    if (command->ordertype.order == COMMAND_MILITARY_PARADE)
    {
        return TRUE;
    }
    return FALSE;
}

void setMilitaryParade(struct CommandToDo *command)
{
    MilitaryParadeCommand *militaryParade = command->militaryParade;
    Ship *leader = militaryParade->aroundShip;
    sdword paradeType = militaryParade->paradeType;
    ParadeTypeInfo *paradeTypeInfo = &paradeTypeInfos[paradeType];
    vector up,right,heading;
    sdword slot;
    sdword subslot;
    MilitarySlot *thisslot;
    SlotInfo *thisslotinfo;
    vector positionat;
    vector positionchange;
    Ship *ship;

    dbgAssertOrIgnore(paradeType >= 0);
    dbgAssertOrIgnore(paradeType < NUMBER_PARADE_TYPES);

    matGetVectFromMatrixCol1(up,leader->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol2(right,leader->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol3(heading,leader->rotatable.rotinfo.coordsys);

    // for each slot, position the ship correctly

    for (slot=0;slot<MAX_MILITARY_SLOTS;slot++)
    {
        thisslot = militaryParade->militarySlots[slot];

        if (thisslot->highestSlotUsed < 0)
        {
            continue;
        }

        thisslotinfo = &paradeTypeInfo->slotinfos[slot];

        // calculate starting positonat of this slot
        // CAT: in the exe's order of the sums (heading + right + up + position; heading + up + right), as
        //      setMilitaryParade 0x00485790 and processMilitaryParadeToDo 0x00485280 compute them
        catParadeSlotPosition(&positionat,&positionchange,&leader->obj.posinfo.position,&up,&right,&heading,thisslotinfo);

        dbgAssertOrIgnore(thisslot->highestSlotUsed < thisslot->numSubslots);

        for (subslot=0;subslot<=thisslot->highestSlotUsed;subslot++)
        {
            ship = thisslot->subslots[subslot].ship;
            if (ship != NULL)
            {
                cat_univSetShipPosition(ship,&positionat);      // CAT: setMilitaryParade 0x00485970
                switch (thisslotinfo->facedirection)
                {
                    vector temp1,temp2;

                    case 0:     // forward
                        ship->rotatable.rotinfo.coordsys = leader->rotatable.rotinfo.coordsys;
                        break;

                    case 1:     // back
                        vecCopyAndNegate(temp1,heading);
                        vecCopyAndNegate(temp2,right);
                        matCreateMatFromVecs(&ship->rotatable.rotinfo.coordsys,&up,&temp2,&temp1);
                        break;

                    case 2:     // right
                        vecCopyAndNegate(temp1,heading);
                        matCreateMatFromVecs(&ship->rotatable.rotinfo.coordsys,&up,&temp1,&right);
                        break;

                    case 3:     // left
                        vecCopyAndNegate(temp2,right);
                        matCreateMatFromVecs(&ship->rotatable.rotinfo.coordsys,&up,&heading,&temp2);
                        break;

                    default:
                        break;      // CAT: no assertion (0x00485985)
                }
                univUpdateObjRotInfo((SpaceObjRot *)ship);
            }

            vecAddTo(positionat,positionchange);
        }
    }
}

void FreeMilitaryParadeContents(MilitaryParadeCommand *militaryParade)
{
    sdword i;

    for (i=0;i<MAX_MILITARY_SLOTS;i++)
    {
        dbgAssertOrIgnore(militaryParade->militarySlots[i]);
        memFree(militaryParade->militarySlots[i]);
        militaryParade->militarySlots[i] = NULL;        // CAT: FreeMilitaryParadeContents 0x00485b00 clears the slot
    }
}

/*-----------------------------------------------------------------------------
    Name        : GetShipSlot
    Description : returns the correct slot a ship belongs to
    Inputs      : ship; militaryParade - CAT: the parade it joins (carriers fill their race's three carrier slots in
                  turn, and a Beast heavy cruiser gets its own slot only around a Beast mothership)
    Outputs     :
    Return      : SLOT_xxx
----------------------------------------------------------------------------*/
// CAT: GetShipSlot 0x00485b30, one case per Cataclysm ship type (a switch on ship->shiptype, ship+0x2B0), with the parade
//      as a second argument (PutShipInMilitaryParade 0x00485ee0 pushes it: 0x00485efb push esi, 0x00485efc push ebx).
//      The exe's two debug messages ("Assigning Swarmer to SLOT_SLEECH", "This ship is using MISC formation slot")
//      are compiled out (nullsubReturnFalse). tResourceCollector and every type without a case use SLOT_Misc.
sdword GetShipSlot(Ship *ship, MilitaryParadeCommand *militaryParade)
{
    switch (ship->shiptype)
    {
        case sRecon:                return SLOT_SRECON;
        case sAcolyte:
        case dAcolyte:              return SLOT_SACOLYTE;
        case sMimic:                return SLOT_SMIMIC;
        case sLeech:                return SLOT_SLEECH;
        case sACV:                  return SLOT_SACV;
        case sMCV:                  return SLOT_SMCV;
        case sRammingFrigate:       return SLOT_SRAMMINGFRIGATE;
        case sMultiBeamFrigate:     return SLOT_SMULTIBEAMFRIGATE;
        case sHiveFrigate:          return SLOT_SHIVEFRIGATE;
        case sSwarmer:
        case bSwarmer:              return SLOT_SLEECH;
        case sDreadnought:          return SLOT_SDREADNOUGHT;
        case sCarrier:
            if (militaryParade->militarySlots[SLOT_SCARRIER0]->highestSlotUsed >= 0)
            {
                return (militaryParade->militarySlots[SLOT_SCARRIER1]->highestSlotUsed >= 0) ? SLOT_SCARRIER2 : SLOT_SCARRIER1;
            }
            return SLOT_SCARRIER0;
        case sWorker:               return SLOT_SWORKER;
        case sProcessor:            return SLOT_SPROCESSOR;
        case sSentinel:             return SLOT_SSENTINEL;
        case sMothership:           return SLOT_SMOTHERSHIP;
        case sDestroyer:            return SLOT_SDESTROYER;
        case bRecon:                return SLOT_BRECON;
        case bAcolyte:              return SLOT_BACOLYTE;
        case bInterceptor:          return SLOT_BINTERCEPTOR;
        case bCloakedFighter:       return SLOT_BCLOAKEDFIGHTER;
        case bAttackBomber:         return SLOT_BATTACKBOMBER;
        case bACV:                  return SLOT_BACV;
        case bMultiGunCorvette:     return SLOT_BMULTIGUNCORVETTE;
        case bHeavyCorvette:        return SLOT_BHEAVYCORVETTE;
        case bMultiBeamFrigate:     return SLOT_BMULTIBEAMFRIGATE;
        case bHiveFrigate:          return SLOT_BHIVEFRIGATE;
        case bIonArrayFrigate:      return SLOT_BIONARRAYFRIGATE;
        case bDFGFrigate:           return SLOT_BDFGFRIGATE;
        case bHeavyCruiser:
            return (militaryParade->aroundShip->shiptype == bMothership) ? SLOT_BHEAVYCRUISER : SLOT_Misc;
        case bCarrier:
            if (militaryParade->militarySlots[SLOT_BCARRIER0]->highestSlotUsed >= 0)
            {
                return (militaryParade->militarySlots[SLOT_BCARRIER1]->highestSlotUsed >= 0) ? SLOT_BCARRIER2 : SLOT_BCARRIER1;
            }
            return SLOT_BCARRIER0;
        case bRammingFrigate:       return SLOT_BRAMMINGFRIGATE;
        case bWorker:               return SLOT_BWORKER;
        case bProcessor:            return SLOT_BPROCESSOR;
        case bSentinel:             return SLOT_BSENTINEL;
        case bMothership:           return SLOT_BMOTHERSHIP;
        case bCruiseMissile:        return SLOT_BCRUISEMISSILE;
        case bMissileCorvette:      return SLOT_BMISSILECORVETTE;
        case kAttackBomber:
        case tAttackBomber:         return SLOT_Fighter4;
        case kCarrier:              return SLOT_Carrier;
        case kCloakGenerator:
        case kGravWellGenerator:
        case tCloakGenerator:
        case tGravWellGenerator:    return SLOT_NonCombat;
        case kHeavyCorvette:        return SLOT_Corvette1;
        case kHeavyCruiser:
        case tHeavyCruiser:         return SLOT_HeavyCruiser;
        case kHeavyDefender:
        case tHeavyDefender:        return SLOT_Fighter1;
        case kHeavyInterceptor:
        case tHeavyInterceptor:     return SLOT_Fighter3;
        case kMultiGunCorvette:
        case tMultiGunCorvette:     return SLOT_Corvette2;
        case kSalCapCorvette:
        case tSalCapCorvette:       return SLOT_SupportCorvette;
        case kStandardDestroyer:
        case tStandardDestroyer:    return SLOT_Destroyer;
        case tMinelayerCorvette:    return SLOT_Corvette3;
        case p1Fighter:             return SLOT_P1Fighter;
        case p1IonArrayFrigate:     return SLOT_P1IonArrayFrigate;
        case p1MissileCorvette:     return SLOT_P1MissileCorvette;
        case p1StandardCorvette:    return SLOT_P1StandardCorvette;
        default:                    return SLOT_Misc;
    }
}

void CalculateNewHighestUsedSlot(MilitarySlot *militarySlot)
{
    sdword i;

    for (i=militarySlot->numSubslots-1;i>=0;i--)
    {
        if (militarySlot->subslots[i].ship != NULL)
        {
            militarySlot->highestSlotUsed = i;
            return;
        }
    }
    militarySlot->highestSlotUsed = -1;
}

void RemoveShipFromMilitaryParade(Ship *shiptoremove,MilitaryParadeCommand *militaryParade)
{
    sdword slot;
    sdword subslot;
    MilitarySlot *militarySlot;

    for (slot=0;slot<MAX_MILITARY_SLOTS;slot++)
    {
        militarySlot = militaryParade->militarySlots[slot];

        dbgAssertOrIgnore(militarySlot->highestSlotUsed < militarySlot->numSubslots);

        for (subslot=0;subslot<=militarySlot->highestSlotUsed;subslot++)
        {
            if (militarySlot->subslots[subslot].ship == shiptoremove)
            {
                // found shiptoremove
                militarySlot->subslots[subslot].ship = NULL;
                CalculateNewHighestUsedSlot(militarySlot);
                return;
            }
        }
    }

    dbgAssertOrIgnore(FALSE);       // should never reach here
}

/*-----------------------------------------------------------------------------
    Name        : PutShipInMilitaryParade
    Description : puts a ship into the correct position in a militaryParade
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void PutShipInMilitaryParade(Ship *ship,MilitaryParadeCommand *militaryParade)
{
    sdword slot;
    MilitarySlot *militarySlot;
    sdword i;
    sdword paradeType = militaryParade->paradeType;
    ParadeTypeInfo *paradeTypeInfo = &paradeTypeInfos[paradeType];
    SlotInfo *slotinfo;

    sdword numSubslotsToAllocate;
    sdword sizeofNewSlot;
    MilitarySlot *newMilitarySlot;

    slot = GetShipSlot(ship, militaryParade);               // CAT: the parade is the second argument (0x00485efb)
    dbgAssertOrIgnore(slot >= 0);
    dbgAssertOrIgnore(slot < MAX_MILITARY_SLOTS);
    slotinfo = &paradeTypeInfo->slotinfos[slot];
    if ((slotinfo->upoffset == 0.0f) && (slotinfo->rightoffset == 0.0f) && (slotinfo->headingoffset == 0.0f))
    {
        // this slot isn't defined well, perhaps a captured ship in another race's parade?  Put in MiscSlot
        slot = SLOT_Misc;
    }
    militarySlot = militaryParade->militarySlots[slot];

    // find first empty subslot
    for (i=0;i<militarySlot->numSubslots;i++)
    {
        if (militarySlot->subslots[i].ship == NULL)
        {
            // found a empty slot
            militarySlot->subslots[i].ship = ship;
            CalculateNewHighestUsedSlot(militarySlot);
            return;
        }
    }

    // did not find a empty slot.  All full.  We should allocate more slots.

    numSubslotsToAllocate = militarySlot->numSubslots + SUBSLOT_BATCH;
    sizeofNewSlot = sizeofMilitarySlot(numSubslotsToAllocate);
    newMilitarySlot = memAlloc(sizeofNewSlot,"newmilslot",0);

    newMilitarySlot->numSubslots = numSubslotsToAllocate;
    for (i=0;i<militarySlot->numSubslots;i++)
    {
        newMilitarySlot->subslots[i] = militarySlot->subslots[i];
    }
    dbgAssertOrIgnore(i == militarySlot->numSubslots);
    for ( ;i<numSubslotsToAllocate;i++)
    {
        newMilitarySlot->subslots[i].ship = NULL;
    }

    // put ship in:
    newMilitarySlot->subslots[militarySlot->numSubslots].ship = ship;
    newMilitarySlot->highestSlotUsed = militarySlot->numSubslots;

    memFree(militarySlot);
    militaryParade->militarySlots[slot] = newMilitarySlot;
}

/*-----------------------------------------------------------------------------
    Name        : AddShipToMilitaryGroup
    Description : adds a ship to a military group command
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void AddShipToMilitaryGroup(ShipPtr ship,struct CommandToDo *militaryGroup)
{
    AddShipToGroup(ship,militaryGroup);

    dbgAssertOrIgnore(militaryGroup->ordertype.order == COMMAND_MILITARY_PARADE);

    PutShipInMilitaryParade(ship,militaryGroup->militaryParade);
}

/*-----------------------------------------------------------------------------
    Name        : CreateMilitaryGroupAroundShip
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
struct CommandToDo *CreateMilitaryGroupAroundShip(struct CommandLayer *comlayer,ShipPtr ship,ShipPtr aroundShip)
{
    CommandToDo *newcommand;
    sdword i,j;
    MilitarySlot *militarySlot;
    SelectCommand *selection;
    MilitaryParadeCommand *militaryParade;

    newcommand = memAlloc(sizeof(CommandToDo),"ToDo",NonVolatile);
    InitCommandToDo(newcommand);

    selection = memAlloc(sizeofSelectCommand(1),"ToDoSelection",NonVolatile);
    selection->numShips = 1;
    selection->ShipPtr[0] = ship;

    newcommand->selection = selection;
    newcommand->ordertype.order = COMMAND_MILITARY_PARADE;
    newcommand->ordertype.attributes = 0;

    newcommand->militaryParade = militaryParade = memAlloc(sizeof(MilitaryParadeCommand),"milparadecom",0);
    militaryParade->aroundShip = aroundShip;
    for (i=0;i<MAX_MILITARY_SLOTS;i++)
    {
        militarySlot = memAlloc(sizeofMilitarySlot(SUBSLOT_BATCH),"milslot",0);
        militarySlot->numSubslots = SUBSLOT_BATCH;
        militarySlot->highestSlotUsed = -1;
        for (j=0;j<SUBSLOT_BATCH;j++)
        {
            militarySlot->subslots[j].ship = NULL;
        }
        militaryParade->militarySlots[i] = militarySlot;
    }

    militaryParade->paradeType = -1;
    // CAT: the parade type is chosen by ship type alone (CreateMilitaryGroupAroundShip 0x00486050, switch on
    //      aroundShip->shiptype); HW1 chose by race, then Mothership/Carrier
    switch (aroundShip->shiptype)
    {
        case sCarrier:
            militaryParade->paradeType = PARADE_SCARRIER;
            break;
        case sProcessor:
        case dEcclesiamInfected:
            militaryParade->paradeType = PARADE_SPROCESSOR;
            break;
        case sMothership:
        case trFloatingCity:
        case trGhostship:
            militaryParade->paradeType = PARADE_SMOTHERSHIP;
            break;
        case bHeavyCruiser:
            militaryParade->paradeType = PARADE_BHEAVYCRUISER;
            break;
        case bCarrier:
            militaryParade->paradeType = PARADE_BCARRIER;
            break;
        case bProcessor:
            militaryParade->paradeType = PARADE_BPROCESSOR;
            break;
        case bMothership:
        case dBeastMothership:
        case dKushanCarrier:
            militaryParade->paradeType = PARADE_BMOTHERSHIP;
            break;
        case kCarrier:
            militaryParade->paradeType = PARADE_R1CARRIER;
            break;
        case tCarrier:
        case trResearchStation:
            militaryParade->paradeType = PARADE_R2CARRIER;
            break;
        case p1Outpost:
        case p1Mothership:
            militaryParade->paradeType = PARADE_P1MOTHERSHIP;
            break;
        case dRKO:
        case dPrisonShip:
        case dPrisonShipInfected:
            militaryParade->paradeType = PARADE_DPRISONSHIP;
            break;
        case dSlipgateSmall:
        case dSlipgateMedium:
        case dSlipgateLarge:
            militaryParade->paradeType = PARADE_SLIPGATE;
            break;
    }

    dbgAssertOrIgnore(militaryParade->paradeType != -1);

    PutShipInMilitaryParade(ship,militaryParade);

    PrepareShipsForCommand(newcommand,TRUE);

    listAddNode(&comlayer->todolist,&newcommand->todonode,newcommand);

    return newcommand;
}

void lockFormation(CommandToDo *formationcommand,udword specialEffect)
{
    //copy leader coordinate system...
    //maybe...but probably don't want to relock formations over and over
    vector heading;
    //dbgAssertOrIgnore(formationcommand->formation.formationLocked == FALSE);
    formationcommand->formation.coordsys = formationcommand->selection->ShipPtr[0]->rotatable.rotinfo.coordsys;
    if(specialEffect == 1)
    {
        //reverse Heading
        matGetVectFromMatrixCol3(heading,formationcommand->formation.coordsys);
        vecScalarMultiply(heading,heading,-1.0f);
        matPutVectIntoMatrixCol3(heading,formationcommand->formation.coordsys);
    }
    formationcommand->formation.formationLocked = TRUE;
}

void unlockFormation(CommandToDo *formationcommand)
{
    //copy leader coordinate system...
    //maybe...but probably don't want to unlock formations over and over
    //it shows we don't know what we're doing :)
    //dbgAssertOrIgnore(formationcommand->formation.formationLocked == TRUE);
    formationcommand->formation.formationLocked = FALSE;
}
