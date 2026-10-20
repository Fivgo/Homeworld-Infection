// =============================================================================
//  AIUtilities.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/1/1998 by fpoiker
// =============================================================================

#include "AIUtilities.h"

#include "AIFeatures.h"
#include "AIHandler.h"
#include "AIMoves.h"
#include "AIPlayer.h"
#include "AIShip.h"
#include "Alliance.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "CommandWrap.h"
#include "FastMath.h"
#include "Randy.h"
#include "Select.h"
#include "Ships.h"
#include "Stats.h"
#include "Tactics.h"
#include "Leech.h"          // CAT: cat_leechIsLeeching (aiuWrapAttack 0x0041ed00)
#include "Mimic.h"

#include <string.h>



//later put this into the scripts
#define AIU_MOTHERSHIP_VALUE    5500

/*-----------------------------------------------------------------------------
    CAT (M2-R7 oracle-rung-aiship): the exe converts a rating to an integer with the double-magic sequence
    fadd qword [0x0072a458] (6755399441055744.0 = 1.5 * 2^52); fstp qword; mov the low dword (aiuRateShip 0x0041acdd,
    aiuRateBlobs 0x0041fd90): the value rounded to the nearest integer, ties to even (the FPU's default rounding), where
    HW1's (udword) cast truncated
----------------------------------------------------------------------------*/
udword cat_aiuRoundToUdword(double value)
{
    double biased = value + 6755399441055744.0;
    uint64_t bits;

    memcpy(&bits, &biased, sizeof(bits));
    return (udword)bits;
}

/*-----------------------------------------------------------------------------
    CAT (M2-R7 oracle-rung-aiship): the exe drops a target that is hidden, in hyperspace or not targetable by the
    computer player from the target list in place, moving the last target into its slot (aiuWrapAttack 0x0041ed83,
    aiuWrapSpecial 0x0041f2e0; the test is aiuShipIsntTargetable's). Returns the number of targets left
----------------------------------------------------------------------------*/
static sdword cat_aiuRemoveUntargetableTargets(SelectCommand *targets)
{
    sdword i;

    for (i = 0; i < targets->numShips; i++)
    {
        if (aiuShipIsntTargetable(targets->ShipPtr[i], aiCurrentAIPlayer->player))
        {
            targets->ShipPtr[i] = targets->ShipPtr[targets->numShips - 1];
            targets->numShips--;
            i--;
        }
    }
    return targets->numShips;
}


/*=============================================================================
    Structures:
=============================================================================*/
//structure for keeping track of enemy blobs and such stuff
typedef struct
{
    udword numBlobs;
    blob   *blob[1];
} blob_array;

//computer player version of blobs - much smaller, and updated differently
typedef struct
{
    vector         centre;
    real32         radius;
    udword         primaryenemystrength;
    udword         primaryenemyvalue;
    udword         otherenemystrength;
    udword         otherenemyvalue;
    udword         goodGuystrength;
    udword         goodGuyvalue;
    udword         goodGuyResourcers;
    bool32           visibility;
    bool32           mothership;  //whether or not there is a mothership in this blob
    SelectCommand *blobShips;   //level 2 information
} aiblob;

//structure for keeping track of enemy aiblobs and such stuff
typedef struct
{
    udword numBlobs;
    aiblob *blob[1];
} aiblob_array;


/*=============================================================================
    AI Utility Variables:
=============================================================================*/
static aiblob_array *aiuEnemyBlobs;
static aiblob_array *aiuGoodGuyBlobs;
static aiblob       *myMothershipBlob = NULL;

//static blob_array *aiuEnemyBlobs;
//static blob_array *aiuGoodGuyBlobs;
//static blob       *myMothershipBlob = NULL;


/*=============================================================================
    Coordinate and Vector Utility Functions:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : aiuGenerateRandomVector
    Description : Generates a random vector
    Inputs      :
    Outputs     :
    Return      : The random vector
----------------------------------------------------------------------------*/
vector aiuGenerateRandomVector(void)
{
    vector randvec;

    randvec.x = (real32)frandyrandombetween(RANDOM_AI_PLAYER,-10,10);
    randvec.y = (real32)frandyrandombetween(RANDOM_AI_PLAYER,-10,10);
    randvec.z = (real32)frandyrandombetween(RANDOM_AI_PLAYER,-10,10);

    return randvec;
}


/*-----------------------------------------------------------------------------
    Name        : aiuGenerateRandomStandoffPoint
    Description : Generates a random point a certain distance away from the center.
    Inputs      : center - the point to standoff from
                  radius - the standoff distance
                  origin - determines near or far side standoff points
                  flags  - modifiers to constrain the random coordinates
    Outputs     :
    Return      : The new coordinates
----------------------------------------------------------------------------*/
vector aiuGenerateRandomStandoffPoint(vector center, real32 radius,
                                      vector origin, udword flags)
{
    vector standoffvec, standoffloc, originvec;
    real32 dotproduct;

    standoffvec = aiuGenerateRandomVector();
    vecCopyAndNormalize(&standoffvec, &standoffloc);
    vecMultiplyByScalar(standoffloc, radius);
    vecAddTo(standoffloc, center);

    switch (flags)
    {
        case RSP_NORMAL:
            break;
        case RSP_NEAR:
            vecSub(originvec, origin, center);
            dotproduct = vecDotProduct(standoffvec, originvec);
            if (dotproduct < 0)
            {
                return aiuGenerateRandomStandoffPoint(center, radius, origin, flags);
            }
            break;
        case RSP_FAR:
            vecSub(originvec, origin, center);
            dotproduct = vecDotProduct(standoffvec, originvec);
            if (dotproduct > 0)
            {
                return aiuGenerateRandomStandoffPoint(center, radius, origin, flags);
            }
            break;
        default:
            break;
    }
    return standoffloc;
}


/*-----------------------------------------------------------------------------
    Name        : aiuGenerateFlankCoordinates
    Description : Finds the coordinates of the flank of another set of coordinates
    Inputs      : center - the center of the flank (where the target of the
                           attack would be)
                  origin - where the attacking ships would be
                  referencevec - a vector that determines what the flank should
                                 be perpendicular to (if 0,0,0 this function
                                 randomizes a replacement
                  radius - how far out the flank should be
    Outputs     :
    Return      : The new coordinates
----------------------------------------------------------------------------*/
vector aiuGenerateFlankCoordinates(vector center, vector origin, vector referencevec, real32 radius)
{
    vector zerovec = {0,0,0}, flankvec, attackvec;

    if (vecAreEqual(zerovec, referencevec))
    {
        referencevec = aiuGenerateRandomVector();
    }

    vecSub(attackvec, origin, center);
    vecCrossProduct(flankvec, attackvec, referencevec);
    vecNormalize(&flankvec);
    vecMultiplyByScalar(flankvec, radius);
    vecAddTo(flankvec, center);

    return flankvec;
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindDistanceSquared
    Description : Finds the distance between two points (leaves the distance
                  squared to reduce computing time)
    Inputs      : point1, point2 - the two points
    Outputs     :
    Return      : the distance squared
----------------------------------------------------------------------------*/
double aiuFindDistanceSquared(vector point1, vector point2)
{
    // CAT (M2-R10 oracle-rung-physics): the exe (aiuFindDistanceSquared 0x0041a2c0) returns the unrounded double
    //      (dz*dz + dy*dy) + dx*dx of the double differences on the FPU (HW1 returned the float sums). Its 35 call
    //      sites in the exe (and the callers in this file, into which the exe inlined it) store it as a float, compare it
    //      unrounded or hand it to fsqrt as a double: the C callers do the same through the double return (a real32
    //      assignment rounds once, a compare is made in double); aitTeamIsFinishedMoving's range * range and
    //      aimProcessMoveTo's square root are written out at their sites
    double dx = (double)point1.x - (double)point2.x;
    double dy = (double)point1.y - (double)point2.y;
    double dz = (double)point1.z - (double)point2.z;

    return (dz * dz + dy * dy) + dx * dx;
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindRangeStandoffPoint
    Description : Finds a point that is within a range of a given destination
    Inputs      : destination - the point to standoff from
                  location - the current starting location
                  range - the distance to stand off
    Outputs     :
    Return      : The standoff point
----------------------------------------------------------------------------*/
vector aiuFindRangeStandoffPoint(vector destination, vector location, real32 range)
{
    real32 dist_from_location;
    vector standoff_point;
    vector standoff_point_err;
    matrix mat;
    real32 errorangle = frandyrandombetween(RANDOM_AI_PLAYER,DEG_TO_RAD(AIU_STANDOFF_NEG_ERROR_ANGLE),DEG_TO_RAD(AIU_STANDOFF_POS_ERROR_ANGLE));
    real32 disterror = frandyrandombetween(RANDOM_AI_PLAYER,AIU_STANDOFF_DIST_ERROR_LOW,AIU_STANDOFF_DIST_ERROR_HIGH);

    vecSub(standoff_point, destination, location);
    dist_from_location = fsqrt(vecMagnitudeSquared(standoff_point));
    dist_from_location -= range;

    if (dist_from_location <= 0)
    {
        return destination;
    }

    vecNormalizeToLength(&standoff_point, dist_from_location * disterror);

    matMakeRotAboutZ(&mat,(real32)cos(errorangle),(real32)sin(errorangle));
    matMultiplyMatByVec(&standoff_point_err, &mat, &standoff_point);

    vecAddTo(standoff_point_err, location);

    return standoff_point_err;
}


/*-----------------------------------------------------------------------------
    Name        : aiuCreatePathStruct
    Description : Creates an empty path structure of a variable size
    Inputs      : numPoints - the size of the path structure,
                  closed - whether the path is an open or closed loop
    Outputs     : Allocates memory for the path structure
    Return      : Pointer to the new path
----------------------------------------------------------------------------*/
Path *aiuCreatePathStruct(udword numPoints, bool32 closed)
{
    udword i;
    vector initVec = {-REALlyBig, -REALlyBig, -REALlyBig};
    Path *newpath  = (Path *)memAlloc(sizeof_Path(numPoints), "newPath", 0);

    for (i = 0; i < numPoints; i++)
    {
        newpath->point[i] = initVec;
    }

    newpath->numPoints = numPoints;
    newpath->closed     = closed;

    return newpath;
}

//
//  allocates memory for and returns a new path (a perfect copy of the existing path)
//
Path *aiuPathDupe(Path *existing)
{
    Path *newPath  = (Path *)memAlloc(sizeof_Path(existing->numPoints), "patrolpath", 0);
    memcpy(newPath, existing, sizeof_Path(existing->numPoints));
    return newPath;
}

/*-----------------------------------------------------------------------------
    Name        : aiuAddPointToPath
    Description : Adds a point to the path
    Inputs      : point - the point to add
                  pointnum - the location of the point in the loop
                  path - the path to add the point to
    Outputs     : Changes one of the point variables in the path structure
    Return      : void
----------------------------------------------------------------------------*/
void aiuAddPointToPath(vector point, udword pointnum, Path *path)
{
    dbgAssertOrIgnore(pointnum < path->numPoints);

    path->point[pointnum] = point;
}


/*-----------------------------------------------------------------------------
    Name        : aiuPointIsInFront
    Description : Returns TRUE if point A is in front of point B, using point B as a reference for "behind point B"
    Inputs      : a - the test point,
                  b - the point to test from,
                  c - the reference point for "behind point b"
    Outputs     :
    Return      : TRUE if point A is in front of point B
----------------------------------------------------------------------------*/
bool32 aiuPointIsInFront(vector a, vector b, vector c)
{
    vector abvect, cbvect;

    vecSub(abvect, a, b);
    vecSub(cbvect, c, b);

    return (vecDotProduct(abvect, cbvect) < 0);
}


/*-----------------------------------------------------------------------------
    Name        : aiuGenerateCircularPath
    Description : Creates a path around a circle
    Inputs      : num_points, center, radius (pretty self explanitory)
    Outputs     : Creates a new path
    Return      : the new path
----------------------------------------------------------------------------*/
Path *aiuGenerateCircularPath(udword num_points, vector center, real32 radius, bool32 closed)
{
    udword i;
    double theta = 0.0;
    vector temp;
    Path *returnPath = aiuCreatePathStruct(num_points, closed);

    for (i=0; i < num_points; i++)
    {
        vecSet(temp, ((real32)(sin(theta))), ((real32)(cos(theta))), 0.0);
        vecNormalize(&temp);
        vecMultiplyByScalar(temp, radius);
        vecAddTo(temp, center);
        aiuAddPointToPath(temp, i, returnPath);
        theta += 2.0 * PI / (double)num_points;
    }

    return returnPath;
}




/*=============================================================================
    General Utility Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : aiuMoveShipSelection
    Description : Moves a ship from one selection to another
    Inputs      : dest - the destination selection
                  source - the source selection
                  num - the ship number to move
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aiuMoveShipSelection(MaxSelection *dest, MaxSelection *source, udword num)
{
    if (source->ShipPtr[num] != NULL)
    {
        selSelectionAddSingleShip(dest, source->ShipPtr[num]);
        clRemoveShipFromSelection(source, source->ShipPtr[num]);
    }
}


/*-----------------------------------------------------------------------------
    Name        : aiuAlreadyHasShipType
    Description : Returns TRUE if the current aiplayer already has numships of shiptype
    Inputs      :
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
bool32 aiuAlreadyHasShipType(ShipType shiptype, udword num)
{
    SelectCommand *newships = aiCurrentAIPlayer->newships.selection;
    udword i;

    for (i=0;i<newships->numShips;i++)
    {
        if (newships->ShipPtr[i]->shiptype == shiptype)
        {
            num--;

            if (num==0)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}



/*=============================================================================
    ShipRelated Utility Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : aiuRescueShip
    Description : Rescues a specific ship
    Inputs      : ship - the ship to rescue, team - the team to do it
    Outputs     : Creates a bunch of moves if needed
    Return      : TRUE if the team is sent off for a rescue
----------------------------------------------------------------------------*/
bool32 aiuRescueShip(ShipPtr ship, struct AITeam *team)
{
    SelectCommand *enemyShips;
    AITeamMove *newMove, *thisMove = team->curMove;
    TypeOfFormation formation;

    enemyShips = aiuFindAttackingShips(ship);

    //later add an override handler for interrupt - gets ships to break off defense
    //for a larger threat, or for attacking
    if ((enemyShips->numShips > 0) &&
        (aitCheckIfOtherDefTeamAnsweringSignalNeedsHelp(team, enemyShips)))
    {
        if ((team->shipList.selection->numShips < AIU_RESCUE_MULTIPLE_MINSHIPS) && (enemyShips->numShips < AIU_RESCUE_ENEMYMULTIPLE_MINSHIPS))
        {
            formation = AIU_RESCUE_NOMULTIPLE_FORMATION;
        }
        else if (enemyShips->numShips < AIU_RESCUE_ENEMYMULTIPLE_MINSHIPS)
        {
            formation = AIU_RESCUE_TEAMMULTIPLE_FORMATION;
        }
        else
        {
            formation = AIU_RESCUE_BOTHMULTIPLE_FORMATION;
        }

        // CAT: a single-player script team attacks with its own tactics (AITeam +0x74, the tree's kasTactics), every
        //      other team aggressively (aiuRescueShip 0x0041a700; the same choice recurs in the exe's AI moves)
        newMove         = aimCreateAdvancedAttackNoAdd(team, selectMemDupSelection(enemyShips, "dupresc", 0), formation,
                                                       (singlePlayerGame && (team->teamType == ScriptTeam)) ? team->kasTactics : Aggressive,
                                                       TRUE, TRUE);
        newMove->events = thisMove->events;
        if (aiuAttackFeatureEnabled(AIA_KAMIKAZE))
        {
            if (aitTeamShipClassIs(CLASS_Corvette, team))
            {
                aieHandlerSetHealthLow(newMove, AIO_CORVETTE_KAMIKAZE_HEALTH, TRUE, FALSE, aihKamikazeHealthLowHandler);
            }
            else if (aitTeamShipClassIs(CLASS_Fighter, team))
            {
                aieHandlerSetHealthLow(newMove, AIO_CORVETTE_KAMIKAZE_HEALTH, TRUE, FALSE, aihKamikazeHealthLowHandler);
            }
        }

        newMove->events.interrupt.handler  = NULL;
        newMove->events.numbersLow.handler = NULL;
        team->curMove->processing = FALSE;
        aitAddmoveBeforeAndMakeCurrent(team, newMove, thisMove);
        aiumemFree(enemyShips);
        return TRUE;
    }
    aiumemFree(enemyShips);
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aiuRescueShipType
    Description : Looks for and sends the team off to rescue a certain type of ship
    Inputs      : selection - the selection to check for the shiptype and rescue,
                  team - the team to do the rescuing,
                  type - the shiptype to rescue
    Outputs     : Creates a bunch of moves if needed
    Return      : a pointer to the ship that's being rescued
----------------------------------------------------------------------------*/
ShipPtr aiuRescueShipType(SelectCommand *selection, struct AITeam *team, ShipType type)
{
    real32 dist = REALlyBig, tempdist;
    ShipPtr ShipInDistress, ChosenShip = NULL;
    vector teamPos = team->shipList.selection->ShipPtr[0]->obj.posinfo.position;

    ShipInDistress = FindFirstInstanceOfShipType(selection, type);

    while (ShipInDistress)
    {
        if ((tempdist = aiuFindDistanceSquared(ShipInDistress->obj.posinfo.position, teamPos)) < dist)
        {
            ChosenShip = ShipInDistress;
        }

        selSelectionRemoveSingleShip((MaxSelection *)selection, ShipInDistress);
        ShipInDistress = FindFirstInstanceOfShipType(selection, type);
    }

    if (ChosenShip)
    {
        if (aiuRescueShip(ChosenShip, team))
        {
            return ChosenShip;
        }
    }

    return NULL;
}


/*-----------------------------------------------------------------------------
    Name        : aiuTakeoutShip
    Description : Takes out a specific ship
    Inputs      : ship - the ship to takeout
                  team - the team to do it
    Outputs     : Creates a bunch of moves if needed
    Return      : TRUE if the team is sent off for taking out
----------------------------------------------------------------------------*/
bool32 aiuTakeoutShip(ShipPtr ship, struct AITeam *team)
{
    SelectCommand *enemyShips;
    AITeamMove *newMove, *thisMove = team->curMove;
    TypeOfFormation formation;

    enemyShips = aiuFindNearbyDangerousEnemyShips(ship, AIU_TAKEOUT_ENEMYFLEET_RADIUS);
    selSelectionAddSingleShip((MaxSelection *)enemyShips, ship);

    //later add an override handler for interrupt - gets ships to break off defense
    //for a larger threat, or for attacking
    if (aitCheckIfOtherDefTeamAnsweringSignalNeedsHelp(team, enemyShips))
    {
        if ((team->shipList.selection->numShips < AIU_TAKEOUT_MULTIPLE_MINSHIPS) &&
            (enemyShips->numShips < AIU_TAKEOUT_ENEMYMULTIPLE_MINSHIPS))
        {
            formation = AIU_TAKEOUT_NOMULTIPLE_FORMATION;
        }
        else if (enemyShips->numShips < AIU_TAKEOUT_ENEMYMULTIPLE_MINSHIPS)
        {
            formation = AIU_TAKEOUT_TEAMMULTIPLE_FORMATION;
        }
        else
        {
            formation = AIU_TAKEOUT_BOTHMULTIPLE_FORMATION;
        }

        newMove         = aimCreateAdvancedAttackNoAdd(team, selectMemDupSelection(enemyShips, "duptake", 0), formation, Aggressive, TRUE, TRUE);
        newMove->events = thisMove->events;
        newMove->events.interrupt.handler  = NULL;
        newMove->events.numbersLow.handler = NULL;
        team->curMove->processing = FALSE;
        aitAddmoveBeforeAndMakeCurrent(team, newMove, thisMove);
        aiumemFree(enemyShips);
        return TRUE;
    }
    aiumemFree(enemyShips);
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aiuTakeoutShipType
    Description : Looks for and sends the team off to takeout a certain type of ship
    Inputs      : selection - the selection to check for the shiptype and takeout,
                  team - the team to do the takeouting,
                  type - the shiptype to takeout
    Outputs     : Creates a bunch of moves if needed
    Return      : a pointer to the ship that's being taken out
----------------------------------------------------------------------------*/
ShipPtr aiuTakeoutShipType(SelectCommand *selection, struct AITeam *team, ShipType type)
{
    real32 tempdist;
    ShipPtr InvaderShip, ChosenShip = NULL;
    vector teamPos = team->shipList.selection->ShipPtr[0]->obj.posinfo.position;

    InvaderShip = FindFirstInstanceOfShipType(selection, type);

    while (InvaderShip)
    {
        if ((tempdist = aiuFindDistanceSquared(InvaderShip->obj.posinfo.position, teamPos)))
        {
            ChosenShip = InvaderShip;
        }

        selSelectionRemoveSingleShip((MaxSelection *)selection, InvaderShip);
        InvaderShip = FindFirstInstanceOfShipType(selection, type);
    }

    if (ChosenShip)
    {
        if (aiuTakeoutShip(ChosenShip, team))
        {
            return ChosenShip;
        }
    }
    return NULL;
}


/*-----------------------------------------------------------------------------
    Name        : aiuShipsNoLongerAttacking
    Description : Checks the ships orders to see if they're still attacking
    Inputs      : ships - the ships to check
    Outputs     :
    Return      : TRUE if the ships are no longer attacking
----------------------------------------------------------------------------*/
bool32 aiuShipsNoLongerAttacking(SelectCommand *ships)
{
    CommandToDo *command;
    udword i;

    for (i=0;i<ships->numShips;i++)
    {
        command = getShipAndItsCommand(&universe.mainCommandLayer,ships->ShipPtr[i]);

        if ((command != NULL) && (command->ordertype.order == COMMAND_ATTACK))
        {
            return FALSE;
        }
    }
    return TRUE;
}



/*-----------------------------------------------------------------------------
    Name        : aiuShipIsAnEnemyMothership
    Description : Returns whether a ship is an enemy mothership or not
    Inputs      : ship - the ship to test
    Outputs     :
    Return      : TRUE if "ship" is an enemy mothership
----------------------------------------------------------------------------*/
bool32 aiuShipIsAnEnemyMothership(Ship *ship)
{
    Player *aiplayerplayer = aiCurrentAIPlayer->player;

    if ((ship == ship->playerowner->PlayerMothership) &&
        (!allianceArePlayersAllied(ship->playerowner, aiplayerplayer)))   //aialliance
    {
        return TRUE;
    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aiuShipIsntAnEnemyMothership
    Description : Returns whether a ship isn't an enemy mothership or not
    Inputs      : ship - the ship to test
    Outputs     :
    Return      : TRUE if "ship" isn't an enemy mothership
----------------------------------------------------------------------------*/
bool32 aiuShipIsntAnEnemyMothership(Ship *ship)                          //aialliance
{
    return (!aiuShipIsAnEnemyMothership(ship));
}



/*-----------------------------------------------------------------------------
    Name        : aiuAnyShipsAreCapitalShips
    Description : Returns true if any of the ships in the selection are capital ships
    Inputs      : ships - the ships to check
    Outputs     :
    Return      : TRUE if any of the selected ships are capital ships
----------------------------------------------------------------------------*/
bool32 aiuAnyShipsAreCapitalShips(SelectCommand *ships)
{
    sdword i;

    for (i=0;i<ships->numShips;i++)
    {
        if (isCapitalShip(ships->ShipPtr[i]))
        {
            return TRUE;
        }
    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aiuShipIsFighter
    Description : Returns TRUE if the ship is a fighter
    Inputs      : ship - the ship to check
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
bool32 aiuShipIsFighter(Ship *ship)
{
    return (ship->staticinfo->shipclass == CLASS_Fighter);
}


/*-----------------------------------------------------------------------------
    Name        : aiuShipNotGoodAgainstFighters
    Description : Returns TRUE if the ship is no good against fighters
    Inputs      : ship - the ship to check
    Outputs     :
    Return      : TRUE if the ship is no good against fighters
----------------------------------------------------------------------------*/
bool32 aiuShipNotGoodAgainstFighters(Ship *ship)
{
    // CAT: both Taiidan and Kushan variants and the Beast's ion array frigate and heavy cruiser
    //      (aiuShipNotGoodAgainstFighters 0x0041aa90: types 0x2c 0x2e 0x42 0x45 0x49 0x50 0x53 0x5a)
    switch (ship->shiptype)
    {
        case bIonArrayFrigate:
        case bHeavyCruiser:
        case kHeavyCruiser:
        case kIonCannonFrigate:
        case kStandardDestroyer:
        case tHeavyCruiser:
        case tIonCannonFrigate:
        case tStandardDestroyer:
            return TRUE;
            break;
        default:
            break;
    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aiuSelectionNotGoodAtKillingTheseTargets
    Description : Checks for a fighter/slow moving capital ship combo, compares relative
                  fleet strengths of selections and returns TRUE if "selection" is
                  stronger than "target"
    Inputs      : selection - the selection to check
                  targets - the targets to check
                  strengthratio - how much stronger selection has to be to return TRUE
    Outputs     :
    Return      : TRUE if the selection is not good at killing the target
----------------------------------------------------------------------------*/
bool32 aiuSelectionNotGoodAtKillingTheseTargets(SelectCommand *selection, SelectCommand *targets, real32 strengthratio)
{
    real32 strength = 0;

    //simple check against fighter/slow moving capital ship matches
    if (DoAllShipsFollowConstraints(targets,aiuShipIsFighter))
    {
        if (DoAllShipsFollowConstraints(selection,aiuShipNotGoodAgainstFighters))
        {
            return TRUE;
        }
    }

    strength = statsGetRelativeFleetStrengths(targets, selection);

    if (strength < strengthratio)
    {
        return TRUE;
    }

    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aiuShipsAreHidden
    Description : Tells whether any of the selection of ships are hidden
    Inputs      : selection - the ships to check
    Outputs     :
    Return      : TRUE if one of the ships are hidden
----------------------------------------------------------------------------*/
bool32 aiuShipsAreHidden(SelectCommand *selection)
{
    udword i;

    for (i = 0; i < selection->numShips; i++)
    {
        if (aiuShipIsHidden(selection->ShipPtr[i]))
        {
            return TRUE;
        }
    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aiuFilterDisabledShips
    Description : Filters out any ship that's disabled
    Inputs      : selection - the ships to filter
                  filtered  - the selection to filter
    Outputs     :
    Return      : Number of non-disabled ships
----------------------------------------------------------------------------*/
udword aiuFilterDisabledShips(SelectCommand *selection, MaxSelection *filtered)
{
    udword i;

    for (i = 0; i < selection->numShips; i++)
    {
        if (!bitTest(selection->ShipPtr[0]->header.flags, SOF_Disabled|SOF_Crazy))
        {
            selSelectionAddSingleShip(filtered, selection->ShipPtr[i]);
        }
    }
    return filtered->numShips;
}


/*-----------------------------------------------------------------------------
    Name        : aiuFilterSelectableShips
    Description : Filters out any ship that isn't selectable
    Inputs      : selection - the ships to filter
                  filtered  - the selection to filter
    Outputs     :
    Return      : Number of selectable ships
----------------------------------------------------------------------------*/
udword aiuFilterSelectableShips(SelectCommand *selection, MaxSelection *filtered)
{
    udword i;

    for (i = 0; i < selection->numShips; i++)
    {
        if (!aiuShipIsntSelectable(selection->ShipPtr[i]))
        {
            selSelectionAddSingleShip(filtered, selection->ShipPtr[i]);
        }
    }
    return filtered->numShips;
}



/*-----------------------------------------------------------------------------
    Name        : aiuShipsArentTargetable
    Description : Tells whether any of the selection of ships isn't selectable
    Inputs      : selection - the ships to check
    Outputs     :
    Return      : TRUE if one of the ships isn't selectable
----------------------------------------------------------------------------*/
bool32 aiuShipsArentTargetable(SelectCommand *selection)
{
    udword i;

    for (i = 0; i < selection->numShips; i++)
    {
        if (aiuShipIsntTargetable(selection->ShipPtr[i], aiCurrentAIPlayer->player))
        {
            return TRUE;
        }
    }
    return FALSE;
}



/*-----------------------------------------------------------------------------
    Name        : aiuIsShipDangerous
    Description : Returns TRUE if ship is a dangerous ship
    Inputs      : ship
    Outputs     :
    Return      : Returns TRUE if ship is a dangerous ship
----------------------------------------------------------------------------*/
bool32 aiuIsShipDangerous(Ship *ship)
{
    ShipStaticInfo *shipstatic = ship->staticinfo;

    switch (shipstatic->shipclass)
    {
        case CLASS_Mothership:
            return FALSE;
        case CLASS_HeavyCruiser:
        case CLASS_Carrier:
        case CLASS_Destroyer:
        case CLASS_MonsterShip:     // CAT: Cataclysm's monster and micro classes are dangerous (aiuIsShipDangerous 0x0041ac40)
        case CLASS_Micro:
            return TRUE;
            break;
        case CLASS_Frigate:
            // CAT: both DFG frigates (0x2d bDFGFrigate, 0x4e tDFGFrigate; 0x0041ac40)
            if ((shipstatic->shiptype == bDFGFrigate) || (shipstatic->shiptype == tDFGFrigate))
            {
                return FALSE;
            }
            else
            {
                return TRUE;    // all other Frigates including AdvanceSupportFrigate are considered dangerous
            }
            break;
        case CLASS_Corvette:
            return TRUE;        // CAT: no HW1 RepairCorvette (M2-R4 ships-b)
        case CLASS_Fighter:
            // CAT: no HW1 DefenseFighter (M2-R4 ships-b)
            {
                return TRUE;
            }
        case CLASS_Resource:
        case CLASS_NonCombat:
        default:
            return FALSE;
    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aiuIsShipPrimaryEnemy
    Description : Returns true if the ship is a primary enemy
    Inputs      : ship - the ship to test
    Outputs     :
    Return      : TRUE if the ship is a primary enemy
----------------------------------------------------------------------------*/
bool32 aiuIsShipPrimaryEnemy(Ship *ship)
{
    if (ship->playerowner == aiCurrentAIPlayer->primaryEnemyPlayer)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


/*-----------------------------------------------------------------------------
    Name        : aiuRateShip
    Description : Rates a ship based on overall strength (from stats.c) and
                  value (based on scriptset values)
    Inputs      : strength, value
    Outputs     : fills in strength and value
    Return      : void
----------------------------------------------------------------------------*/
void aiuRateShip(udword *strength, udword *value, ShipPtr ship)
{
    // CAT: only Sect and Beast ships have a strength, their kill rating rounded to the nearest integer; a ship of
    //      class Mothership or a trFloatingCity is worth AIU_MOTHERSHIP_VALUE, any other ship of another race 1
    //      (aiuRateShip 0x0041acb0), where HW1 tested the FloatingCity and Mothership types
    if ((ship->shiprace == RACE_Sect) || (ship->shiprace == RACE_Beast))
    {
        *strength = cat_aiuRoundToUdword((double)statsGetOverallKillRating(ship->staticinfo));
    }
    else
    {
        *strength = 0;
    }
    if ((ship->staticinfo->shipclass == CLASS_Mothership) ||
        (ship->shiptype == trFloatingCity))
    {
        *value = AIU_MOTHERSHIP_VALUE;
    }
    else if ((ship->shiprace == RACE_Sect) || (ship->shiprace == RACE_Beast))
    {
        *value = ship->staticinfo->buildCost;
    }
    else
    {
        *value = 1;
    }
/*    switch (ship->shiptype)
    {
        case Mothership:
            value = AIU_MOTHERSHIP_VALUE;
            break;

        case HeavyCruiser:
            value = AIU_HEAVYCRUISER_VALUE;
            break;

        case Carrier:
            value = AIU_CARRIER_VALUE;
            break;

        case StandardDestroyer:
        case MissileDestroyer:
            value = AIU_DESTROYER_VALUE;
            break;

        case AdvanceSupportFrigate:
        case DFGFrigate:
        case IonCannonFrigate:
        case StandardFrigate:

        case CloakGenerator:
        case GravWellGenerator:

        case AttackBomber:
        case CloakedFighter:
        case HeavyDefender:
        case HeavyInterceptor:

        case HeavyCorvette:
        case MinelayerCorvette:
        case MultiGunCorvette:

        case ProximitySensor:
        case ResourceCollector:
        case SalCapCorvette:

    }
*/
}


/*-----------------------------------------------------------------------------
    Name        : aiuEnemyShipsInMothershipBlob
    Description : Determines if enemy ships are in the mothership blob
    Inputs      :
    Outputs     :
    Return      : Returns TRUE if enemy ships were found
----------------------------------------------------------------------------*/
SelectCommand *aiuEnemyShipsInMothershipBlob(void)
{
    udword i;
    SelectCommand *enemyships = NULL;
    MaxSelection tempsel;

    if (!myMothershipBlob)
    {
        return NULL;
    }

    tempsel.numShips = 0;

    for (i = 0; i < myMothershipBlob->blobShips->numShips; i++)
    {
        // CAT: the crystals do not count (aiuEnemyShipsInMothershipBlob 0x0041ad40: types 0x7e, 0x7f)
        if ((!allianceArePlayersAllied(myMothershipBlob->blobShips->ShipPtr[i]->playerowner, aiCurrentAIPlayer->player)) &&
            (myMothershipBlob->blobShips->ShipPtr[i]->shiptype != dCrystal0) &&
            (myMothershipBlob->blobShips->ShipPtr[i]->shiptype != dCrystal1))
        {
            selSelectionAddSingleShip(&tempsel, myMothershipBlob->blobShips->ShipPtr[i]);
        }
    }

    if (tempsel.numShips)
    {
        enemyships = selectMemDupSelection((SelectCommand *)&tempsel, "esimb", 0);
    }

    return enemyships;
}


/*-----------------------------------------------------------------------------
    Name        : aiuMakeShipsOnlyDangerousToMothership
    Description : Filters out ships that pose no risk to the mothership
    Inputs      : ships - a selection of ships to filter
    Outputs     : Reduces a selection of ships
    Return      : void
----------------------------------------------------------------------------*/
void aiuMakeShipsOnlyDangerousToMothership(SelectCommand *ships)
{
    bool32 dangerous = FALSE;
    udword i;

    for (i = 0; i < ships->numShips; dangerous = FALSE)
    {
        switch (ships->ShipPtr[i]->staticinfo->shipclass)
        {
            case CLASS_HeavyCruiser:
            case CLASS_Carrier:
            case CLASS_Destroyer:
                dangerous = TRUE;
                break;
            case CLASS_Frigate:
                // CAT: neither support frigate (aiuMakeShipsOnlyDangerousToMothership 0x0041ae00: 0x3c, 0x4a)
                if ((ships->ShipPtr[i]->shiptype != kAdvanceSupportFrigate) &&
                    (ships->ShipPtr[i]->shiptype != tAdvanceSupportFrigate))
                {
                    dangerous = TRUE;
                }
                break;
            case CLASS_Corvette:
                if (ships->ShipPtr[i]->shiptype == tMinelayerCorvette)
                {
                    dangerous = TRUE;
                }
                else if (ships->numShips > 3)
                {
                    dangerous = TRUE;
                }
                break;
            case CLASS_Fighter:
                // CAT: both attack bombers (0x3d, 0x4b)
                if (((ships->ShipPtr[i]->shiptype == kAttackBomber) || (ships->ShipPtr[i]->shiptype == tAttackBomber)) &&
                    (ships->numShips > 5))
                {
                    dangerous = TRUE;
                }
                break;
            case CLASS_NonCombat:
            case CLASS_Resource:
            default:
                break;

        }

        if (!dangerous)
        {
            clRemoveShipFromSelection(ships, ships->ShipPtr[i]);
        }
        else
        {
            i++;
        }
    }
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindNearestMothershipAttacker
    Description : Finds the nearest enemy ship to the team in the mothership blob
    Inputs      : team - the team which is looking for the attacker
    Outputs     :
    Return      : The nearest ship
----------------------------------------------------------------------------*/
ShipPtr aiuFindNearestMothershipAttacker(SelectCommand *teamShips)
{
    ShipPtr nearestship = NULL;
    real32 nearestshipdist = REALlyBig, newshipdist;
    SelectCommand *enemyships;
    udword i;
    vector approxTeamPosition;

    if ((!aiCurrentAIPlayer->shipsattackingmothership) ||
        (!aiCurrentAIPlayer->shipsattackingmothership->numShips))
    {
        return NULL;
    }

    dbgAssertOrIgnore(teamShips->numShips);

    approxTeamPosition = teamShips->ShipPtr[0]->obj.posinfo.position;

    enemyships = selectMemDupSelection(aiCurrentAIPlayer->shipsattackingmothership, "fnma", Pyrophoric);

    aiuMakeShipsOnlyDangerousToMothership(enemyships);

    //if there are only smaller, not as dangerous, ships attacking the mothership
    //set enemyships back to the original structure
    if (!enemyships->numShips)
    {
        memFree(enemyships);
        enemyships = selectMemDupSelection(aiCurrentAIPlayer->shipsattackingmothership, "fnm2", Pyrophoric);
    }

    for (i = 0; i < enemyships->numShips; i++)
    {
        if (!nearestship)
        {
            nearestship     = enemyships->ShipPtr[i];
            nearestshipdist = aiuFindDistanceSquared(nearestship->obj.posinfo.position, approxTeamPosition);
        }
        else
        {
            newshipdist = aiuFindDistanceSquared(enemyships->ShipPtr[i]->obj.posinfo.position, approxTeamPosition);

            if (nearestshipdist > newshipdist)
            {
                nearestship     = enemyships->ShipPtr[i];
                nearestshipdist = newshipdist;
            }
        }
    }
    memFree(enemyships);
    return nearestship;
}


/*-----------------------------------------------------------------------------
    Name        : aiuPlayerMothershipCoords
    Description : Returns the coordinates of the player's mothership or 0,0,0 if the mothership is dead
    Inputs      : player - the player who's mothership we're looking for
    Outputs     :
    Return      : the player's mothership location
----------------------------------------------------------------------------*/
vector aiuPlayerMothershipCoords(Player *player)
{
    vector originvect = {0,0,0};

    if (player->PlayerMothership)
    {
        return player->PlayerMothership->obj.posinfo.position;
    }
    return originvect;
}



/*-----------------------------------------------------------------------------
    Name        : aiuFindEnemyMothership
    Description : Finds the primary enemy's mothership
    Inputs      : player - current player
    Outputs     :
    Return      : Returns the enemy mothership of player
----------------------------------------------------------------------------*/
Ship *aiuFindEnemyMothership(Player *player)
{
    udword i;
    Player *enemyplayer;

    //if the player already has a primary enemy
    if (player->aiPlayer)
    {
        return player->aiPlayer->primaryEnemyPlayer->PlayerMothership;
    }
    else
    {
        for (i=0;i<universe.numPlayers;i++)
        {
            enemyplayer = &universe.players[i];
            if (enemyplayer != player)
            {
                return enemyplayer->PlayerMothership;
            }
        }
    }
    return NULL;
}

/*-----------------------------------------------------------------------------
    Name        : aiuFindEnemyMothershipCoords
    Description : Finds the coordinates of the enemy mothership
    Inputs      : player - the player who's enemies' mothership the function
                           should find
    Outputs     :
    Return      : coordinates of the enemy mothership
----------------------------------------------------------------------------*/
vector aiuFindEnemyMothershipCoords(Player *player)
{
    Ship *ship = aiuFindEnemyMothership(player);

    if (ship != NULL)
    {
        return ship->obj.posinfo.position;
    }
    else
    {
        vector zero = { 0.0f, 0.0f, 0.0f };
        aiplayerLog((aiIndex,"Warning: Couldn't find enemy mothership"));
        return zero;
    }
}


// CAT (M2-R7 ai-orders-moves): no aiuShipsCloseToEnemyMothership: cataclysm.exe has none and its callers (HW1's
//      aiaProcessHarassTeams, the P2 swarm attack) are gone with it


/*-----------------------------------------------------------------------------
    Name        : aiuFindSafestStandoffPoint
    Description : Finds the safest point for a ship to standoff from a set of ships
    Inputs      : ships - the ships to generate the standoff point from
                  distance - the standoff distance
    Outputs     : Creates a location coordinates
    Return      : the location coordinates or ***zero coordinates*** if all directions
                  are safe
----------------------------------------------------------------------------*/
vector aiuFindSafestStandoffPoint(SelectCommand *ships, real32 distance)
{
    real32 temp, distsq = distance*distance, enemyblob_distsq, enemyblob_radiussq, max_distsq;
    udword i;
    aiblob *enemyblob;
    vector center, cumulative_dirvect, sharedblob_dirvect, temp_dirvect, master_dirvect;

    vecZeroVector(sharedblob_dirvect);
    vecZeroVector(cumulative_dirvect);
    center     = selCentrePointComputeGeneral((MaxSelection *)ships, &temp);
    max_distsq = AIU_SAFEST_STANDOFF_DIST_MULT * distsq;

    for (i=0;i<aiuEnemyBlobs->numBlobs;i++)
    {
        enemyblob          = aiuEnemyBlobs->blob[i];
        enemyblob_radiussq = enemyblob->radius*enemyblob->radius;
        enemyblob_distsq   = aiuFindDistanceSquared(center, enemyblob->centre);

        //if the enemy blob is close enough
        if (enemyblob_distsq< max_distsq)
        {
            //but the center of the selected ships isn't in the blob
            if (enemyblob_distsq > enemyblob_radiussq)
            {
                vecSub(temp_dirvect, enemyblob->centre, center);
                vecDivideByScalar(temp_dirvect, enemyblob_distsq, temp);
                vecAddTo(cumulative_dirvect, temp_dirvect);
            }
            else
            {
                //center of selected ships is in the blob
                //store this vector for if no other nearby blobs are found
                vecSub(sharedblob_dirvect, enemyblob->centre, center);
            }
        }
    }
    if (!vecIsZero(cumulative_dirvect))
    {
        vecCopyAndNormalize(&cumulative_dirvect, &master_dirvect);
    }
    else if (!vecIsZero(sharedblob_dirvect))
    {
        vecCopyAndNormalize(&sharedblob_dirvect, &master_dirvect);
    }
    else
    {
        //return zero vector
        return sharedblob_dirvect;
    }

    //negate the distance, because we want the ship to be on the
    //other side of the bad dudes
    vecMultiplyByScalar(master_dirvect, -distance);
    vecAddTo(master_dirvect, center);

    return master_dirvect;
}



/*-----------------------------------------------------------------------------
    Name        : aiuGeneratePatrolPath
    Description : Generates a path for a patrol to follow while patrolling
                  the computer player's fleet
    Inputs      :
    Outputs     :
    Return      : Path for the patrol to follow
----------------------------------------------------------------------------*/
Path *aiuGeneratePatrolPath(udword patroltype)
{
    SelectCommand *vulnerable_ships;
    udword i;
    Path *patrol_path;
    vector patrol_point, enemyMothershipPos, aiMothershipPos;
    ShipPtr aiMothership = aiCurrentAIPlayer->player->PlayerMothership,
            enemyMothership = aiuFindEnemyMothership(aiCurrentAIPlayer->player);

    if ((!aiMothership) || (!enemyMothership))
    {
        return aiuCreatePathStruct(0, FALSE);
    }

    enemyMothershipPos = enemyMothership->obj.posinfo.position;
    aiMothershipPos    = aiMothership->obj.posinfo.position;

    switch (patroltype)
    {
        case AIT_FAST_PATROL:
            vulnerable_ships = aiuFindClosestVulnerableGoodGuyShips(enemyMothership, ALL_VULNERABLE_SHIPS);
            break;
        case AIT_SLOW_PATROL:
            vulnerable_ships = aiuFindClosestVulnerableGoodGuyShips(enemyMothership, LARGE_VULNERABLE_SHIPS);
            break;
        default:
            aiplayerLog((aiIndex,"Patrol Type not found, using fast"));
            vulnerable_ships = aiuFindClosestVulnerableGoodGuyShips(enemyMothership, ALL_VULNERABLE_SHIPS);
            break;
    }

    vulnerable_ships = aiuRejectShipsPastLocation(vulnerable_ships, enemyMothershipPos, aiMothershipPos);

    if (vulnerable_ships)
    {
        patrol_path = aiuCreatePathStruct(vulnerable_ships->numShips, FALSE);
        for (i = 0; i < vulnerable_ships->numShips; i++)
        {
            patrol_point = aiuFindRangeStandoffPoint(vulnerable_ships->ShipPtr[i]->obj.posinfo.position, enemyMothershipPos, FASTPATROL_STANDOFFDIST);
            aiuAddPointToPath(patrol_point, i, patrol_path);
        }
        memFree(vulnerable_ships);
        return patrol_path;
    }
    else
    {
        return aiuCreatePathStruct(0, FALSE);
    }
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindUnarmedUndefendedEnemyShips
    Description : Finds a selection Unarmed Undefended Enemy Ship,
                  hence the name of the function
    Inputs      :
    Outputs     :
    Return      : The selection of UnarmedUndefendedEnemyShips that were found
----------------------------------------------------------------------------*/
SelectCommand *aiuFindUnarmedUndefendedEnemyShips(void)
{
    sdword i, j;
    bool32 protection_found = FALSE;
    SelectCommand *unprotected_ships = NULL, *blobShipsPtr;
    udword blob_shipclass;
    ShipPtr blobship;

    for (i = 0; i < aiuEnemyBlobs->numBlobs; i++,protection_found = FALSE)
    {
        //for each blob, check to see if there is a fighting ship in it.  If there
        //is, reject the blob
        if (aiuEnemyBlobs->blob[i]->visibility)
        {
            blobShipsPtr = aiuEnemyBlobs->blob[i]->blobShips;

            for (j = 0; j < blobShipsPtr->numShips; j++)
            {
                blobship       = blobShipsPtr->ShipPtr[j];
                blob_shipclass = blobship->staticinfo->shipclass;

                // CAT: neither salvage corvette nor the crystals and slipgates protect a blob
                //      (aiuFindUnarmedUndefendedEnemyShips 0x0041b690: 0x48 0x59 0x7e 0x7f 0x70 0x71 0x72)
                if ((!allianceArePlayersAllied(blobship->playerowner, aiCurrentAIPlayer->player)) &&
                    (blob_shipclass != CLASS_Resource) &&
                    (blob_shipclass != CLASS_NonCombat) &&
                    (blobship->shiptype != kSalCapCorvette) && (blobship->shiptype != tSalCapCorvette) &&
                    (blobship->shiptype != dCrystal0) && (blobship->shiptype != dCrystal1) &&
                    (blobship->shiptype != dSlipgateSmall) && (blobship->shiptype != dSlipgateMedium) &&
                    (blobship->shiptype != dSlipgateLarge))
                {
                    //reject this blob
                    protection_found = TRUE;
                    break;
                }
            }

            // if the entire selection of ships in a blob are unprotected add the selection
            // to the unprotected_ships selection
            if (!protection_found)
            {
                unprotected_ships = selectMergeTwoSelections(unprotected_ships, aiuEnemyBlobs->blob[i]->blobShips, DEALLOC1);

                //look for any non-enemy ships in the blob and remove them from the selection
                for (j = (unprotected_ships->numShips - 1); j >= 0; j--)
                {
                    MakeTargetsOnlyEnemyShips((SelectAnyCommand *)unprotected_ships, aiCurrentAIPlayer->player);
                }
            }
        }
    }

    return unprotected_ships;
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindUnarmedEnemyShips
    Description : Finds a selection of Unarmed enemy ships
    Inputs      :
    Outputs     :
    Return      : The selection of ships that were found
----------------------------------------------------------------------------*/
SelectCommand *aiuFindUnarmedEnemyShips(void)
{
    sdword i, j;
    SelectCommand *unarmed_ships = NULL, *blobsel = NULL;
    MaxSelection temp_sel;
    udword blob_shipclass;
    ShipPtr blobship;

    temp_sel.numShips = 0;

    for (i = 0; i < aiuEnemyBlobs->numBlobs; i++)
    {
        //For each blob, find the unarmed ships
        if (aiuEnemyBlobs->blob[i]->visibility)
        {
            blobsel = aiuEnemyBlobs->blob[i]->blobShips;

            for (j = 0; j < blobsel->numShips; j++)
            {
                blobship       = blobsel->ShipPtr[j];
                blob_shipclass = blobship->staticinfo->shipclass;

                // CAT: both races' support frigates and salvage corvettes, neither grav well generator
                //      (aiuFindUnarmedEnemyShips 0x0041b7d0: 0x3c 0x4a 0x48 0x59; not 0x40 0x4f)
                if ((!allianceArePlayersAllied(blobship->playerowner, aiCurrentAIPlayer->player)) &&
                    ((blob_shipclass == CLASS_Resource) || (blob_shipclass == CLASS_NonCombat) ||
                     (blobship->shiptype == kAdvanceSupportFrigate) || (blobship->shiptype == tAdvanceSupportFrigate) ||
                     (blobship->shiptype == kSalCapCorvette) || (blobship->shiptype == tSalCapCorvette)) &&
                    (blobship->shiptype != kGravWellGenerator) && (blobship->shiptype != tGravWellGenerator))
                {
                    selSelectionAddSingleShip(&temp_sel, blobship);
                }
            }
        }
    }

    if (temp_sel.numShips)
    {
        unarmed_ships = (SelectCommand *)memAlloc(sizeofSelectCommand(temp_sel.numShips), "unarmedenemy", 0);

        selSelectionCopy((MaxAnySelection *)unarmed_ships, (MaxAnySelection *)&temp_sel);
    }


    return unarmed_ships;

}


/*-----------------------------------------------------------------------------
    Name        : aiuFindFighterVulnerableEnemyShips
    Description : Finds a selection of enemy capital ships vulnerable to fighters
    Inputs      :
    Outputs     :
    Return      : The selection of ships that were found
----------------------------------------------------------------------------*/
SelectCommand *aiuFindFighterVulnerableEnemyShips(void)
{
    sdword i, j;
    SelectCommand *vulnerable_ships = NULL, *blobsel = NULL;
    MaxSelection temp_sel;
    udword blob_shipclass;
    ShipPtr blobship;

    temp_sel.numShips = 0;

    for (i = 0; i < aiuEnemyBlobs->numBlobs; i++)
    {
        //For each blob, find the unarmed ships
        if (aiuEnemyBlobs->blob[i]->visibility)
        {
            blobsel = aiuEnemyBlobs->blob[i]->blobShips;

            for (j = 0; j < blobsel->numShips; j++)
            {
                blobship       = blobsel->ShipPtr[j];
                blob_shipclass = blobship->staticinfo->shipclass;

                if ((!allianceArePlayersAllied(blobship->playerowner, aiCurrentAIPlayer->player)) &&
                    (isCapitalShip(blobship)) &&
                    // CAT: no DFG frigate, grav well generator or mothership of either race
                    //      (aiuFindFighterVulnerableEnemyShips 0x0041b900: 0x4e 0x2d 0x40 0x4f 0x12 0x35)
                    (blobship->shiptype != tDFGFrigate) && (blobship->shiptype != bDFGFrigate) &&
                    (blobship->shiptype != kGravWellGenerator) && (blobship->shiptype != tGravWellGenerator) &&
                    (blobship->shiptype != sMothership) && (blobship->shiptype != bMothership))
                {
                    selSelectionAddSingleShip(&temp_sel, blobship);
                }
            }
        }
    }

    if (temp_sel.numShips)
    {
        vulnerable_ships = (SelectCommand *)memAlloc(sizeofSelectCommand(temp_sel.numShips), "unarmedenemy", 0);

        selSelectionCopy((MaxAnySelection *)vulnerable_ships, (MaxAnySelection *)&temp_sel);
    }


    return vulnerable_ships;

}


/*-----------------------------------------------------------------------------
    Name        : aiuFindBestVolumeToMine
    Description : Finds the best volume to mine
    Inputs      : minelayers - the minelaying team
    Outputs     : Creates a volume
    Return      : the new volume
----------------------------------------------------------------------------*/
Volume *aiuFindBestVolumeToMine(SelectCommand *selection)
{
    vector dest;
    Volume *minevol = NULL;

    if (aiuFindBestResourceBlob(&dest))
    {
        minevol = (Volume *)memAlloc(sizeof(Volume), "voltomine", 0);
        minevol->type = VOLUME_SPHERE;
        minevol->attribs.sphere.center = dest;

        //later make this have more variation
        minevol->attribs.sphere.radius = 8000;
    }

    return minevol;

    //later find other places to mine:
    //  - in between mothership and primary enemy mothership
    //  - mine sphere of influence?  After mothership has stopped moving
    //  - mine resource area close to enemy
}



/*-----------------------------------------------------------------------------
    Name        : aiuFindResourceCollectors
    Description : Finds all of one player's resource collectors
    Inputs      : None - uses aiCurrentAIPlayer to determine resource collectors
    Outputs     :
    Return      : Selection of resouce collectors
----------------------------------------------------------------------------*/
SelectCommand *aiuFindResourceCollectors(void)
{
    return aiCurrentAIPlayer->airResourceCollectors.selection;
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindResourceControllerDestination
    Description : Finds an ideal place for the resource controller to reside
    Inputs      : none
    Outputs     :
    Return      : a vector with the new resource controller destination
----------------------------------------------------------------------------*/
vector aiuFindResourceControllerDestination(void)
{
    udword i, totalResourcers = 0;
    aiblob *blob;
    vector destination;
    real32 tmp;

    vecZeroVector(destination);

    for (i=0;i<aiuGoodGuyBlobs->numBlobs;i++)
    {
        blob = aiuGoodGuyBlobs->blob[i];

        if (blob->goodGuyResourcers && !blob->mothership)
        {
            vecAddToScalarMultiply(destination, blob->centre, blob->goodGuyResourcers);
            totalResourcers += blob->goodGuyResourcers;
        }
    }
    vecDivideByScalar(destination, (real32)totalResourcers, tmp);

    return destination;
}


/*-----------------------------------------------------------------------------
    Name        : aiuDestinationNotNearAnotherMothership
    Description : Makes sure that the destination isn't near the destination or location of another player's mothership
    Inputs      : destination - the chosen destination,
                  radiussq - the distance to check
    Outputs     :
    Return      : TRUE if the destination is fine
----------------------------------------------------------------------------*/
bool32 aiuDestinationNotNearOtherMothership(vector destination, real32 radiussq)
{
    Node *node;
    Ship *ship;

    //  check all ships in the damn universe
    node = universe.ShipList.head;
    while (node != NULL)
    {
        ship = (Ship *)listGetStructOfNode(node);

        //if the ship is a mothership class but doesn't belong to the CPU
        // CAT: the class, not HW1's two types (aiuDestinationNotNearOtherMothership 0x0041bb80: CLASS_Mothership, CLASS_Carrier)
        if (((ship->staticinfo->shipclass == CLASS_Mothership) ||
             (ship->staticinfo->shipclass == CLASS_Carrier)) &&
            (ship->playerowner != aiCurrentAIPlayer->player))    //allianceArePlayersAllied?
        {
            // if the destination is near the destination of the ship,
            //  or near the ship itself
            if (((!vecIsZero(ship->moveTo)) && (aiuFindDistanceSquared(destination, ship->moveTo) < radiussq)) ||
                (aiuFindDistanceSquared(destination, ship->obj.posinfo.position) < radiussq))
            {
                return FALSE;
            }
        }

        node = node->next;
    }
    return TRUE;
}



/*-----------------------------------------------------------------------------
    Name        : aiuFindSlowestShipMaxSpeed
    Description : Finds the maximum speed of the slowest ship in the selection
    Inputs      : ships - the selection of ships
    Outputs     :
    Return      : See "Description"
----------------------------------------------------------------------------*/
real32 aiuFindSlowestShipMaxSpeed(SelectCommand *ships)
{
    udword i;
    real32 tempmaxspeed, slowestmaxspeed = REALlyBig;

    for (i=0;i<ships->numShips;i++)
    {
        // CAT: the top speed carries the speed multiplier (cat_shipGetSpeedMultiplier 0x00514940; aiuFindSlowestShipMaxSpeed 0x0041bcf0)
        // CAT (M2-R10 oracle-rung-physics): the unrounded product is compared and stored when lower (0x0041bd1f ..
        //      0x0041bd31)
        double maxspeedD;

        tempmaxspeed = cat_shipGetSpeedMultiplier(ships->ShipPtr[i]);
        maxspeedD = tacticsGetShipsMaxVelocity(ships->ShipPtr[i]) * tempmaxspeed;

        if (maxspeedD < slowestmaxspeed)
        {
            slowestmaxspeed = (real32)maxspeedD;
        }
    }

    return slowestmaxspeed;
}




/*-----------------------------------------------------------------------------
    Name        : aiuGetClosestShip
    Description : Finds the ship in the selection that is closest to "ship"
    Inputs      : selection - the group of ships to look through, ship -
    Outputs     :
    Return      : the closest ship
----------------------------------------------------------------------------*/
ShipPtr aiuGetClosestShip(SelectCommand *selection, ShipPtr ship)
{
    vector  shippos = ship->obj.posinfo.position;
    real32  closest_distance, distance;
    ShipPtr closest_ship = NULL;
    udword  i;

    if ((selection == NULL) || (!selection->numShips) || (ship == NULL))
    {
        return NULL;
    }

    closest_ship = selection->ShipPtr[0];

    closest_distance = aiuFindDistanceSquared(shippos, closest_ship->obj.posinfo.position);

    for (i = 1; i < selection->numShips; i++)
    {
        if ((distance = aiuFindDistanceSquared(shippos, selection->ShipPtr[i]->obj.posinfo.position)) < closest_distance)
        {
            closest_ship = selection->ShipPtr[i];
        }
    }

    return closest_ship;
}


/*-----------------------------------------------------------------------------
    Name        : aiuShipIsVulnerableGoodGuy
    Description : Returns whether or not the ship is a vulnerable good guy ship
    Inputs      : ship - the ship to check
    Outputs     :
    Return      : TRUE if the ship is a vulnerable good guy ship
----------------------------------------------------------------------------*/
bool32 aiuShipIsVulnerableGoodGuy(ShipPtr ship)
{
    ShipType shiptype = ship->shiptype;
    ShipClass shipclass = ship->staticinfo->shipclass;

    //could have minelayers as part of this test, but
    //since it is used as an attack craft in the game,
    //it's left out
    if ((ship->playerowner == aiCurrentAIPlayer->player) &&
        // CAT: both races' support frigates and salvage corvettes (aiuFindClosestVulnerableGoodGuyShips 0x0041be90 inlines this)
        ((shipclass == CLASS_Resource) ||
         (shiptype  == kAdvanceSupportFrigate) || (shiptype == tAdvanceSupportFrigate) ||
         (shiptype  == kSalCapCorvette) || (shiptype == tSalCapCorvette)))
    {
        return TRUE;
    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aiuShipIsLargeVulnerableGoodGuy
    Description : Returns whether a ship is a large vulnerable good guy ship
    Inputs      : ship - the ship to test
    Outputs     :
    Return      : TRUE if the ship is a large vulnerable good guy ship
----------------------------------------------------------------------------*/
bool32 aiuShipIsLargeVulnerableGoodGuy(ShipPtr ship)
{
    ShipType shiptype = ship->shiptype;

    if ((ship->playerowner == aiCurrentAIPlayer->player) &&
        ((shiptype == kAdvanceSupportFrigate) || (shiptype == tAdvanceSupportFrigate)))     // CAT: both (0x0041be90)
    {
        return TRUE;
    }
    return FALSE;
}



/*-----------------------------------------------------------------------------
    Name        : aiuFindClosestVulnerableGoodGuyShips
    Description : Finds the closest unarmed or lightly armed good guy ships
    Inputs      : ship - the ship the unarmed good guy ship must be close to
    Outputs     :
    Return      : A selection of close vulnerable ships
----------------------------------------------------------------------------*/
SelectCommand *aiuFindClosestVulnerableGoodGuyShips(ShipPtr ship, udword type)
{
    MaxSelection tempsel;
    SelectCommand *targets, *blobShipsPtr;
    ShipPtr tempship, bubbleship;
    udword i, j;
    vector ship_pos = ship->obj.posinfo.position;
    real32 dist, tempdist;

    //initialize structures
    targets = (SelectCommand *)memAlloc(sizeofSelectCommand(aiuGoodGuyBlobs->numBlobs), "cvggs", 0);
    targets->numShips = 0;
    tempsel.numShips  = 0;

    //go through all the good guy blobs
    for (i = 0; i < aiuGoodGuyBlobs->numBlobs; i++)
    {
        //for each blob, pull out the vulnerable ships
        blobShipsPtr = aiuGoodGuyBlobs->blob[i]->blobShips;

        for (j = 0; j < blobShipsPtr->numShips;j++)
        {
            switch (type)
            {
                case ALL_VULNERABLE_SHIPS:
                    if (aiuShipIsVulnerableGoodGuy(blobShipsPtr->ShipPtr[j]))
                    {
                        selSelectionAddSingleShip(&tempsel, blobShipsPtr->ShipPtr[j]);
                    }
                    break;
                case LARGE_VULNERABLE_SHIPS:
                    if (aiuShipIsLargeVulnerableGoodGuy(blobShipsPtr->ShipPtr[j]))
                    {
                        selSelectionAddSingleShip(&tempsel, blobShipsPtr->ShipPtr[j]);
                    }
                    break;
                default:
                    aiplayerLog((aiIndex,"Could not find find type, using ALL_VULNERABLE_SHIPS"));
                    if (aiuShipIsVulnerableGoodGuy(blobShipsPtr->ShipPtr[j]))
                    {
                        selSelectionAddSingleShip(&tempsel, blobShipsPtr->ShipPtr[j]);
                    }
                    break;
            }
        }
    }

    //do the stuff below until we've found enough ships, or until we're done
    while (targets->numShips < aiuGoodGuyBlobs->numBlobs)
    {
        //once the temporary selection is empty, then we're done
        if (tempsel.numShips == 0)
        {
            break;
        }

        bubbleship = tempsel.ShipPtr[0];
        clRemoveShipFromSelection(&tempsel, bubbleship);
        dist = aiuFindDistanceSquared(bubbleship->obj.posinfo.position, ship_pos);

        //use a bubble sort to find the closest ship in tempsel
        for (i = 0; i < tempsel.numShips; i++)
        {
            if ((tempdist = aiuFindDistanceSquared(tempsel.ShipPtr[i]->obj.posinfo.position, ship_pos))
                 < dist)
            {
                //found a closer ship, switch the two
                tempship           = tempsel.ShipPtr[i];
                tempsel.ShipPtr[i] = bubbleship;
                bubbleship         = tempship;
                dist               = tempdist;
            }
        }

        //add the ship we found to the list
        selSelectionAddSingleShip((MaxSelection *)targets, bubbleship);
    }

    if (targets->numShips == 0)
    {
        memFree(targets);
        return NULL;
    }
    else
    {
        return targets;
    }
}


/*-----------------------------------------------------------------------------
    Name        : aiuRejectShipsPastLocation
    Description : Weeds out ships in a selection that are farther away from a reference point than a location
    Inputs      : selection - the selection of ships to check,
                  reference - the point from which the distance checks originate,
                  location - the farthest distance
    Outputs     : Removes ships from a selectcommand
    Return      : The filtered selection of ships
----------------------------------------------------------------------------*/
SelectCommand *aiuRejectShipsPastLocation(SelectCommand *selection, vector reference, vector location)
{
    real32 dist, refdist;
    udword i;

    if (!selection)
    {
        return NULL;
    }

    refdist = aiuFindDistanceSquared(reference, location);

    for (i = 0; i < selection->numShips;)
    {
        dist = aiuFindDistanceSquared(selection->ShipPtr[i]->obj.posinfo.position, reference);

        if (dist > refdist)
        {
            clRemoveShipFromSelection(selection, selection->ShipPtr[i]);
        }
        else
        {
            i++;
        }
    }
    return selection;
}



/*-----------------------------------------------------------------------------
    Name        : aiuFindClosestUnarmedUndefendedEnemyShip
    Description : Title says it all
    Inputs      : The ship that the enemy ship must be close to
    Outputs     :
    Return      : A pointer to the closest unarmed undefended enemy ship,
                  NULL if none was found
----------------------------------------------------------------------------*/
ShipPtr aiuFindClosestUnarmedUndefendedEnemyShip(ShipPtr ship)
{
    SelectCommand *targets;
    ShipPtr punygirlyship;

    targets       = aiuFindUnarmedUndefendedEnemyShips();
    punygirlyship = aiuGetClosestShip(targets, ship);
    aiumemFree(targets);

    return punygirlyship;
}



/*-----------------------------------------------------------------------------
    Name        : aiuFindBestUnarmedEnemyShip
    Description : Finds the unarmed enemy ship that the team can most likely destroy
    Inputs      : ship - the ship that the enemy ship must be close to
                  range - the distance to check for enemy ships around the unarmed ship
    Outputs     :
    Return      : A pointer to the closest unarmed enemy ship, NULL if none was found
----------------------------------------------------------------------------*/
ShipPtr aiuFindBestUnarmedEnemyShip(SelectCommand *teamShips, real32 range)
{
    SelectCommand *targets, *protection = NULL;
    ShipPtr punygirlyship = NULL;
    real32 strength = 0;

    targets = aiuFindUnarmedEnemyShips();

    while ((strength < AIU_FINDUNARMED_PROTECTION_STRENGTH) && (targets) && (targets->numShips > 0))
    {
        punygirlyship = aiuGetClosestShip(targets, teamShips->ShipPtr[0]);

        aiumemFree(protection);
        protection = aiuFindNearbyDangerousEnemyShips(punygirlyship, range);

        if (protection->numShips == 0)
            break;

        strength = statsGetRelativeFleetStrengths(protection, teamShips);

        if (strength < AIU_FINDUNARMED_PROTECTION_STRENGTH)
        {
            selSelectionRemoveSingleShip((MaxSelection *)targets, punygirlyship);
            punygirlyship = NULL;
        }

    }
    aiumemFree(protection);
    aiumemFree(targets);

    return punygirlyship;
}



/*-----------------------------------------------------------------------------
    Name        : aiuFindBestFighterVulnerableEnemyShip
    Description : Finds the enemy ship vulnerable to fighters that the team can
                  most likely destroy
    Inputs      : ship - the ship that the enemy ship must be close to
                  range - the distance to check for enemy ships around the vulnerable ship
    Outputs     :
    Return      : A pointer to the closest unarmed enemy ship, NULL if none was found
----------------------------------------------------------------------------*/
ShipPtr aiuFindBestFighterVulnerableEnemyShip(SelectCommand *teamShips, real32 range)
{
    SelectCommand *targets, *protection = NULL;
    ShipPtr punygirlyship = NULL;
    real32 strength = 0;

    targets = aiuFindFighterVulnerableEnemyShips();

    while ((strength < AIU_FINDUNARMED_PROTECTION_STRENGTH) && (targets) && (targets->numShips > 0))
    {
        punygirlyship = aiuGetClosestShip(targets, teamShips->ShipPtr[0]);

        aiumemFree(protection);
        protection = aiuFindNearbyDangerousEnemyShips(punygirlyship, range);

        if (protection->numShips == 0)
            break;

        strength = statsGetRelativeFleetStrengths(protection, teamShips);

        if (strength < AIU_FINDUNARMED_PROTECTION_STRENGTH)
        {
            selSelectionRemoveSingleShip((MaxSelection *)targets, punygirlyship);
            punygirlyship = NULL;
        }

    }
    aiumemFree(protection);
    aiumemFree(targets);

    return punygirlyship;
}




/*-----------------------------------------------------------------------------
    Name        : aiuFindNearbyEnemyShips
    Description : finds nearby enemy ships (next to primarytarget)
    Inputs      : primarytarget, range
    Outputs     :
    Return      : selection containing nearby enemy ships (next to primarytarget)
----------------------------------------------------------------------------*/
SelectCommand *aiuFindNearbyEnemyShips(Ship *primarytarget,real32 range)
{
    SelectCommand *enemyShips;
    MaxSelection tempShips;
    Ship *ship = getShipNearObjTok((SpaceObjRotImpTarg *)primarytarget, range);

    tempShips.numShips = 0;

    if (ship == NULL)
    {
        enemyShips = (SelectCommand *)memAlloc(sizeofSelectCommand(1), "emptysel", 0);
        enemyShips->numShips = 0;
        return enemyShips;      // done to prevent crash - many places expect non-null to be returned ...Gary
    }

    while (ship != NULL)
    {
        selSelectionAddSingleShip(&tempShips, ship);
        ship = getShipNearObjTok(NULL, range);
    }

    MakeTargetsOnlyEnemyShips((SelectAnyCommand *)&tempShips, aiCurrentAIPlayer->player);
    MakeTargetsOnlyBeWithinRangeAndNotIncludeMe((SelectAnyCommand *)&tempShips, (SpaceObjRotImpTarg *)primarytarget, range);

    //size of function needs to be one more because frequently another ship is added by the calling function
    aiuNewSelection(enemyShips, (tempShips.numShips + 1), "fnes");
    memcpy(enemyShips,&tempShips,sizeofSelectCommand(tempShips.numShips));
    enemyShips->numShips = tempShips.numShips;

//    aiplayerLog((aiIndex, "Used Bryces blob code, found %i ships", enemyShips->numShips));

    return enemyShips;
}

/*-----------------------------------------------------------------------------
    Name        : aiuFindNearbyDangerousEnemyShips
    Description : finds nearby enemy ships (next to primarytarget)
    Inputs      : primarytarget, range
    Outputs     :
    Return      : selection containing nearby enemy ships (next to primarytarget)
----------------------------------------------------------------------------*/
SelectCommand *aiuFindNearbyDangerousEnemyShips(Ship *primarytarget,real32 range)
{
    SelectCommand *dangerousShips = aiuFindNearbyEnemyShips(primarytarget,range);

    if (dangerousShips)
    {
        MakeShipsOnlyFollowConstraints(dangerousShips,aiuIsShipDangerous);
    }

    return dangerousShips;
}


/*-----------------------------------------------------------------------------
    Name        : aiuMakeShipsOnlyPrimaryEnemyShips
    Description : Filters the selection to ensure that the targets are only primary enemies
    Inputs      : selection - the selection to filter
    Outputs     : reduces the size of the selection if needed
    Return      : void
----------------------------------------------------------------------------*/
void aiuMakeShipsOnlyPrimaryEnemyShips(SelectCommand *selection)
{
    MakeTargetsOnlyEnemyShips((SelectAnyCommand *)selection, aiCurrentAIPlayer->player);
    MakeShipsOnlyFollowConstraints(selection,aiuIsShipPrimaryEnemy);
}



/*-----------------------------------------------------------------------------
    Name        : aiuFindAttackingShips
    Description : Finds enemy ships that are attacking the ship
    Inputs      : ship - the ship being attacked
    Outputs     :
    Return      : Selection containing the ships attacking the ship
----------------------------------------------------------------------------*/
SelectCommand *aiuFindAttackingShips(ShipPtr ship)
{
    SelectCommand *attackers = aiuFindNearbyDangerousEnemyShips(ship, AIU_FINDATTACKING_ENEMYFLEET_RADIUS);

    if (ship->gettingrocked)
    {
        if (!attackers)
        {
            aiuNewSelection(attackers, 1, "fas");
        }
        selSelectionAddSingleShip((MaxSelection *)attackers, ship->gettingrocked);
    }

    return attackers;
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindHarassTarget
    Description : Finds a ship that the harass team can annoy.
                  First looks for unarmed undefended ships, then
                  looks for lightly defended and armed ships.
    Inputs      : teamShips - the member of the harass team
    Outputs     :
    Return      : the ship to attack
----------------------------------------------------------------------------*/
ShipPtr aiuFindHarassTarget(SelectCommand *teamShips)
{
    ShipPtr target1 = NULL, target2 = NULL, target3 = NULL;
    real32 target1_dist = REALlyBig, target2_dist = REALlyBig, target3_dist = REALlyBig;
    vector teamPos = teamShips->ShipPtr[0]->obj.posinfo.position;
    ShipPtr enemyMothership = NULL;

    target1 = aiuFindClosestUnarmedUndefendedEnemyShip(teamShips->ShipPtr[0]);
    target2 = aiuFindBestUnarmedEnemyShip(teamShips, AIU_HARASS_PROTECTION_RADIUS);
    target3 = aiuFindBestFighterVulnerableEnemyShip(teamShips, AIU_HARASS_PROTECTION_RADIUS);

    enemyMothership = aiuFindEnemyMothership(aiCurrentAIPlayer->player);
    if (target1 &&
        !(enemyMothership &&
          (target1->staticinfo->shipclass == CLASS_Resource) &&     // CAT: the class (aiuFindHarassTarget 0x0041c620)
          (aiuFindDistanceSquared(target1->obj.posinfo.position, enemyMothership->obj.posinfo.position) < 4500)))
    {
        target1_dist  = aiuFindDistanceSquared(target1->obj.posinfo.position, teamPos);
        target1_dist *= AIU_UNARMED_UNDEFENDED_MODIFIER;
    }
    if (target2 &&
        !(enemyMothership &&
          (target2->staticinfo->shipclass == CLASS_Resource) &&     // CAT: the class (aiuFindHarassTarget 0x0041c620)
          (aiuFindDistanceSquared(target2->obj.posinfo.position, enemyMothership->obj.posinfo.position) < 4500)))
    {
        target2_dist  = aiuFindDistanceSquared(target2->obj.posinfo.position, teamPos);
        target2_dist *= AIU_UNARMED_MODIFIER;
    }
    if (target3 &&
        !(enemyMothership &&
          (target3->staticinfo->shipclass == CLASS_Resource) &&     // CAT: the class (aiuFindHarassTarget 0x0041c620)
          (aiuFindDistanceSquared(target3->obj.posinfo.position, enemyMothership->obj.posinfo.position) < 4500)))
    {
        target3_dist  = aiuFindDistanceSquared(target3->obj.posinfo.position, teamPos);
        target3_dist *= AIU_FIGHTER_VULNERABLE_MODIFIER;
    }

    if ((target1_dist < target2_dist) && (target1_dist < target3_dist))
    {
        return target1;
    }

    if (target2_dist < target3_dist)
    {
        return target2;
    }

    return target3;
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindClosestPrimaryEnemyShipWorthCapturing
    Description : I think the title of this particular function is explicit
                  enough to fully explain the purpose of this particular function
    Inputs      :
    Outputs     :
    Return      : the ship
----------------------------------------------------------------------------*/
ShipPtr aiuFindClosestEnemyShipWorthCapturing(aiblob *blob)
{
    real32 distsq, closestblobdistsq = REALlyBig, closestshipdistsq = REALlyBig;
    // CAT-FIX: aiuFindClosestEnemyShipWorthCapturing 0x0041c990 leaves its result uninitialised when no enemy blob holds
    //          a ship worth capturing (it returns a stack value); NULL here
    ShipPtr closestshipworth = NULL, ship = NULL;
    SelectCommand *blobSelection;
    udword i, j;

    //find the closest enemy blob with a ship worth capturing
    for (i=0;i<aiuEnemyBlobs->numBlobs;i++)
    {
        if ((distsq = aiuFindDistanceSquared(blob->centre, aiuEnemyBlobs->blob[i]->centre)) < closestblobdistsq)
        {
            closestblobdistsq = distsq;
            blobSelection     = aiuEnemyBlobs->blob[i]->blobShips;

            for (j=0;j<blobSelection->numShips;j++)
            {
                ship = blobSelection->ShipPtr[j];

                if ((!allianceArePlayersAllied(ship->playerowner, aiCurrentAIPlayer->player)) &&
                    (aiuShipIsWorthCapturing(ship)) &&
                    ((distsq = aiuFindDistanceSquared(blob->centre, ship->obj.posinfo.position)) < closestshipdistsq))
                {
                    closestshipdistsq = distsq;
                    closestshipworth  = ship;
                }
            }
        }
    }

    return closestshipworth;
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindCaptureTarget
    Description : Finds the juiciest targets for capturing
    Inputs      : capturingShips - the ships that will be capturing
    Outputs     : Creates a selection
    Return      : Selection of ships
                  Note: this function does not return a conventional selection
                  with numShips denoting the number of ship pointers.  In this
                  selection, numShips denotes the number of slots.
                  Slot1: the closest most expensive sighted primary enemy ship
                  Slot2: the closest most expensive sighted non-primary enemy ship
                  Slot3: the closest enemy ship worth capturing
----------------------------------------------------------------------------*/
SelectCommand *aiuFindCaptureTarget(SelectCommand *capturingShips)
{
    SelectCommand *returnSel;
    SelectCommand *highestPriCostSelection = NULL, *highestNonCostSelection = NULL, *enemySelection;
    udword highestPriCost = 0, highestNonCost = 0, i;
    ShipPtr highestPriCostClosestShip = NULL, highestNonCostClosestShip = NULL, ship;
    real32 pridistsq, closestPriDistSq = REALlyBig, nondistsq, closestNonDistSq = REALlyBig;
    aiblob *dablob;

    // CAT: no mothership exclusion (aiuFindCaptureTarget 0x0041cb50 rates every type by its first ship's class and
    //      build cost), and the visibility test asks cat_shipIsTargetableBy (aiuShipIsntTargetable)
    dablob = (aiblob *)aiuFindShipsAIBlob(capturingShips, FALSE);

    dbgAssertOrIgnore(dablob != NULL);

    aiuNewSelection(returnSel, 3, "capturetarget");

    //find the closest most expensive sighted primary and non-primary enemy ships
    for (i=0;i<TOTAL_NUM_SHIPS;i++)
    {
        enemySelection = aiCurrentAIPlayer->primaryEnemyShipsIAmAwareOf[i].selection;

        //find the highest cost sighted primary enemy ships by shiptype
        if ((enemySelection->numShips) &&
            (aiuShipIsWorthCapturing(enemySelection->ShipPtr[0])) &&
            (enemySelection->ShipPtr[0]->staticinfo->buildCost > highestPriCost))
        {
            highestPriCost          = enemySelection->ShipPtr[0]->staticinfo->buildCost;
            highestPriCostSelection = enemySelection;
        }

        enemySelection = aiCurrentAIPlayer->enemyShipsIAmAwareOf[i].selection;

        //find the highest cost sighted non-primary enemy ships by shiptype
        if ((aiCurrentAIPlayer->enemyPlayerCount > 1) &&
            (enemySelection->numShips) &&
            (aiuShipIsWorthCapturing(enemySelection->ShipPtr[0])) &&
            (enemySelection->ShipPtr[0]->staticinfo->buildCost > highestNonCost))
        {
            highestNonCost          = enemySelection->ShipPtr[0]->staticinfo->buildCost;
            highestNonCostSelection = enemySelection;
        }
    }

    if (highestPriCostSelection)
    {
        for (i=0;i<highestPriCostSelection->numShips;i++)
        {
            ship = highestPriCostSelection->ShipPtr[0];

            //find the closet of the highest cost sighted primary enemy ships
            if ((aiuEnemyShipIsVisible(ship)) &&
                ((pridistsq = aiuFindDistanceSquared(dablob->centre, ship->obj.posinfo.position)) < closestPriDistSq))
            {
                closestPriDistSq          = pridistsq;
                highestPriCostClosestShip = highestPriCostSelection->ShipPtr[i];
            }
        }
    }

    // this equals NULL if highestPriCostSelection = NULL
    returnSel->ShipPtr[0] = highestPriCostClosestShip;

    if (highestNonCostSelection)
    {
        for (i=0;i<highestNonCostSelection->numShips;i++)
        {
            ship = highestNonCostSelection->ShipPtr[i];

            //find the closet of the highest cost sighted enemy ships
            if ((aiuEnemyShipIsVisible(ship)) &&
                ((nondistsq = aiuFindDistanceSquared(dablob->centre, ship->obj.posinfo.position)) < closestNonDistSq))
            {
                closestNonDistSq          = nondistsq;
                highestNonCostClosestShip = highestNonCostSelection->ShipPtr[i];
            }
        }
    }
    returnSel->ShipPtr[1] = highestNonCostClosestShip;

    returnSel->ShipPtr[2] = aiuFindClosestEnemyShipWorthCapturing(dablob);

    return returnSel;
}



// CAT (M2-R7 ai-orders-moves): no aiuFindSwarmerTarget / aiuFindSwarmTargets (P2 swarm moves only; not in cataclysm.exe)


/*-----------------------------------------------------------------------------
    Name        : aiuPointWithinSphereOfInfluence
    Description : blah blah blah... see "Name"
    Inputs      : point - the point to check
    Outputs     :
    Return      : TRUE if the point is in the sphere of influence
----------------------------------------------------------------------------*/
bool32 aiuPointWithinSphereOfInfluence(vector point)
{
    vector center;
    real32 radiussq;

    center   = aiuPlayerMothershipCoords(aiCurrentAIPlayer->player);
    radiussq = aiCurrentAIPlayer->sphereofinfluence;

    if (aiuFindDistanceSquared(point, center) < radiussq)
    {
        return TRUE;
    }
    return FALSE;
}



/*-----------------------------------------------------------------------------
    Name        : aiuFindLeadShipInSphereOfInfluence
    Description : Finds the most important enemy ships (one ship per group) that
                  are within the sphere of influence of the computer player
    Inputs      : centre - the centre of the sphere of influence,
                  radiussq - the radius of the sphere squared
    Outputs     : Creates a selection of ships in memory
    Return      : The selection of ships in the sphere of influence
----------------------------------------------------------------------------*/
SelectCommand *aiuFindLeadShipInSphereOfInfluence(vector centre, real32 radiussq)
{
    udword i, j, k;
    SelectCommand *return_sel;
    MaxSelection temp_return_sel, temp_sel;

    temp_return_sel.numShips = 0;

    for (i = 0, j = 0; i < aiuEnemyBlobs->numBlobs; i++)
    {
        if ((aiuEnemyBlobs->blob[i]->visibility) &&
            (aiuFindDistanceSquared(aiuEnemyBlobs->blob[i]->centre, centre) < radiussq))
        {
            selSelectionCopy((MaxAnySelection *)&temp_sel, (MaxAnySelection *)aiuEnemyBlobs->blob[i]->blobShips);

            MakeTargetsOnlyEnemyShips((SelectAnyCommand *)&temp_sel, aiCurrentAIPlayer->player);

            if (!temp_sel.numShips)
            {       //"enemy" ships only consist of allies
                continue;
            }
//            dbgAssertOrIgnore(temp_sel.numShips > 0);

            for (k = 0; k < temp_sel.numShips; k++)
            {
                //find a ship in the blob that has actually crossed into the sphere
                //reject it if it is a fighter among a small group of ships (the other ships will
                //be checked to make sure that we're not being fooled
                if ((aiuFindDistanceSquared(temp_sel.ShipPtr[k]->obj.posinfo.position, centre) < radiussq) &&
                    (!((isShipOfClass(temp_sel.ShipPtr[k], CLASS_Fighter) && temp_sel.numShips < 4))))
                {
                    temp_return_sel.ShipPtr[j] = temp_sel.ShipPtr[k];
                    j++;
                    temp_return_sel.numShips++;

                    //we've found one invader, let's find others from other blobs
                    break;
                }
            }
        }
    }

    if (temp_return_sel.numShips > 0)
    {
        return_sel = selectMemDupSelection((SelectCommand *)&temp_return_sel, "lsoi", Pyrophoric);
        return return_sel;
    }
    return NULL;

}


/*-----------------------------------------------------------------------------
    Name        : aiuFindDecloakedshipInSphereOfInfluence
    Description : Finds a decloaked cloak capable ship, or a decloaking
                  non- cloak capable ship in the sphere of influence
    Inputs      : None - calculates the sphere of influence on its own
    Outputs     :
    Return      : True if a ship is decloaking within the sphere of influence
----------------------------------------------------------------------------*/
bool32 aiuFindDecloakedShipInSphereOfInfluence(void)
{
    udword i, j;
    SelectCommand *temp_sel;
    ShipPtr ship;
    vector center;
    real32 radiussq;

    if (!aiCurrentAIPlayer->player->PlayerMothership)
    {
        return FALSE;
    }

    center = aiCurrentAIPlayer->player->PlayerMothership->obj.posinfo.position;
    radiussq = aiCurrentAIPlayer->sphereofinfluence;

    for (i = 0, j = 0; i < aiuEnemyBlobs->numBlobs; i++)
    {
        if ((aiuEnemyBlobs->blob[i]->visibility) &&
            (aiuFindDistanceSquared(aiuEnemyBlobs->blob[i]->centre, center) < radiussq))
        {
            temp_sel = aiuEnemyBlobs->blob[i]->blobShips;

            dbgAssertOrIgnore(temp_sel->numShips > 0);


            for (j = 0; j < temp_sel->numShips; j++)
            {
                ship = temp_sel->ShipPtr[j];
                //find a ship in the blob that has actually crossed into the sphere
                //that is cloak capable and uncloaked, or a ship that is in the process
                //of decloaking
                // CAT: any enemy ship inside the sphere that is not fully cloaked (cat_shipIsCloakedFully 0x00436110;
                //      aiuFindDecloakedShipInSphereOfInfluence 0x0041d160), where HW1 asked for the cloaking types or a
                //      decloaking ship
                if ((!allianceArePlayersAllied(ship->playerowner, aiCurrentAIPlayer->player)) &&
                    (aiuFindDistanceSquared(ship->obj.posinfo.position, center) < radiussq) &&
                    (!cat_shipIsCloakedFully(ship)))
                {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;

}


/*-----------------------------------------------------------------------------
    Name        : aiuFindStandoffOfNextNearestBlobOut
    Description : Finds a standoff point in the next blob from the source,
                  going outwards, using the holder as a starting point
    Inputs      : source - the ship to move out from,
                  holder - the ship to start with when going outwards
    Outputs     :
    Return      : The standoff point
----------------------------------------------------------------------------*/
vector aiuFindStandoffOfNextNearestBlobOut(ShipPtr source, ShipPtr holder)
{
    real32 ref_distsq,  //distance squared between source and holder
        blob_distsq,    //distance squared between source and test blob
        nearest_distsq = REALlyBig; //smallest distance squared recorded so far
    blob *source_blob = source->obj.collMyBlob, *holder_blob = holder->obj.collMyBlob;
    blob *current_nearest = NULL, *tempBlob;
    Node *blobnode = universe.collBlobList.head;
    vector done_position = ORIGIN_VECTOR;

    ref_distsq = aiuFindDistanceSquared(source_blob->centre, holder_blob->centre);

    while (blobnode != NULL)
    {
        tempBlob = (blob *)listGetStructOfNode(blobnode);

        //don't want to use the same blob we're already in
        if (vecAreEqual(tempBlob->centre, holder_blob->centre))
        {
            blobnode = blobnode->next;
            continue;
        }

        blob_distsq   = aiuFindDistanceSquared(source_blob->centre, tempBlob->centre);

        if ((blob_distsq < nearest_distsq) &&
            (aiuPointIsInFront(tempBlob->centre, holder_blob->centre, source_blob->centre)))
        {
            current_nearest = tempBlob;
            nearest_distsq  = blob_distsq;
        }
        blobnode = blobnode->next;
    }

    if (current_nearest)
    {
        return aiuGenerateRandomStandoffPoint(current_nearest->centre, current_nearest->radius/2,
                                              holder->obj.posinfo.position, RSP_NORMAL);
    }
    else
    {
        return done_position;
    }
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindStandoffOfNextNearestBlobIn
    Description : Finds a standoff point in the next blob from the source,
                  going inwards, using the holder as a starting point
    Inputs      : source - the ship to move in to,
                  holder - the ship to start with when going inwards
    Outputs     :
    Return      : The standoff point
----------------------------------------------------------------------------*/
vector aiuFindStandoffOfNextNearestBlobIn(ShipPtr source, ShipPtr holder)
{
    real32 ref_distsq, blob_distsq, farthest_distsq = 0;
    blob *source_blob = source->obj.collMyBlob, *holder_blob = holder->obj.collMyBlob;
    blob *current_farthest = NULL, *tempBlob;
    Node *blobnode = universe.collBlobList.head;
    vector done_position = ORIGIN_VECTOR;

    ref_distsq = aiuFindDistanceSquared(source_blob->centre, holder_blob->centre);

    while (blobnode != NULL)
    {
        tempBlob = (blob *)listGetStructOfNode(blobnode);

        //don't want to use the same blob we're already in
        if (vecAreEqual(tempBlob->centre,holder_blob->centre))
        {
            blobnode = blobnode->next;
            continue;
        }

        blob_distsq = aiuFindDistanceSquared(source_blob->centre, tempBlob->centre);

        if ((blob_distsq > farthest_distsq) &&
            (!aiuPointIsInFront(tempBlob->centre, holder_blob->centre, source_blob->centre)))
        {
            current_farthest = tempBlob;
            farthest_distsq  = blob_distsq;
        }
        blobnode = blobnode->next;
    }

    if (current_farthest)
    {
        return aiuGenerateRandomStandoffPoint(current_farthest->centre, current_farthest->radius/2,
                                              holder->obj.posinfo.position, RSP_NORMAL);
    }
    else
    {
        return done_position;
    }
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindStandoffOfNextNearestBlobOut
    Description : Finds a standoff point in the next enemy blob from the source,
                  going outwards, using the holder as a starting point
    Inputs      : source - the ship to move out from,
                  holder - the ship to start with when going outwards
    Outputs     :
    Return      : The standoff point
----------------------------------------------------------------------------*/
vector aiuFindStandoffOfNextNearestEnemyBlobOut(ShipPtr source, ShipPtr holder)
{
    real32 ref_distsq,  //distance squared between source and holder
        blob_distsq,    //distance squared between source and test blob
        holder_distsq,  //distance squared between holder and test blob
        nearest_distsq = REALlyBig; //smallest distance squared recorded so far
    blob   *source_blob = source->obj.collMyBlob, *holder_blob = holder->obj.collMyBlob;
    aiblob *current_nearest = NULL, *tempBlob;
    vector done_position = ORIGIN_VECTOR;
    udword i;

    ref_distsq = aiuFindDistanceSquared(source_blob->centre, holder_blob->centre);

    for (i=0; i<aiuEnemyBlobs->numBlobs; i++)
    {
        tempBlob = aiuEnemyBlobs->blob[i];

        //don't want to use the same blob we're already in
        if (vecAreEqual(tempBlob->centre, holder_blob->centre))
        {
            continue;
        }

        blob_distsq   = aiuFindDistanceSquared(source_blob->centre, tempBlob->centre);
        holder_distsq = aiuFindDistanceSquared(holder_blob->centre, tempBlob->centre);

        if ((blob_distsq < nearest_distsq) &&
            (aiuPointIsInFront(tempBlob->centre, holder_blob->centre, source_blob->centre)))
        {
            current_nearest = tempBlob;
            nearest_distsq  = blob_distsq;
        }
    }

    if (current_nearest)
    {
        if (current_nearest->mothership)
        {
            return aiuGenerateRandomStandoffPoint(current_nearest->centre, 15000 /*RECON_STANDOFF_DISTANCE*/,
                                                  holder->obj.posinfo.position, RSP_NEAR);
        }

        return aiuGenerateRandomStandoffPoint(current_nearest->centre, current_nearest->radius/2,
                                              holder->obj.posinfo.position, RSP_NORMAL);
    }
    else
    {
        return done_position;
    }
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindStandoffOfNextNearestEnemyBlobIn
    Description : Finds a standoff point in the next enemy blob from the source,
                  going inwards, using the holder as a starting point
    Inputs      : source - the ship to move in to,
                  holder - the ship to start with when going inwards
    Outputs     :
    Return      : The standoff point
----------------------------------------------------------------------------*/
vector aiuFindStandoffOfNextNearestEnemyBlobIn(ShipPtr source, ShipPtr holder)
{
    real32 ref_distsq, blob_distsq, farthest_distsq = 0;
    blob *source_blob = source->obj.collMyBlob, *holder_blob = holder->obj.collMyBlob;
    aiblob *current_farthest = NULL, *tempBlob;
    vector done_position = ORIGIN_VECTOR;
    udword i;

    ref_distsq = aiuFindDistanceSquared(source_blob->centre, holder_blob->centre);

    for (i=0; i<aiuEnemyBlobs->numBlobs; i++)
    {
        tempBlob = aiuEnemyBlobs->blob[i];

        //don't want to use the same blob we're already in
        if (vecAreEqual(tempBlob->centre,holder_blob->centre))
        {
            continue;
        }

        blob_distsq = aiuFindDistanceSquared(source_blob->centre, tempBlob->centre);

        if ((blob_distsq > farthest_distsq) &&
            (!aiuPointIsInFront(tempBlob->centre, holder_blob->centre, source_blob->centre)))
        {
            current_farthest = tempBlob;
            farthest_distsq  = blob_distsq;
        }
    }

    if (current_farthest)
    {
        return aiuGenerateRandomStandoffPoint(current_farthest->centre, current_farthest->radius/2,
                                              holder->obj.posinfo.position, RSP_NORMAL);
    }
    else
    {
        return done_position;
    }
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindResourcesInVolume
    Description : Returns all the resources in a particular volume
    Inputs      : volume - the volume to check
    Outputs     :
    Return      : A selection of resources
----------------------------------------------------------------------------*/
ResourceSelection *aiuFindResourcesInVolume(Volume volume)
{
    blob *testBlob;
    Node *blobnode = universe.collBlobList.head;
    MaxAnySelection tempSel;
    udword i;

    tempSel.numTargets = 0;

    //for each blob
    while (blobnode != NULL)
    {
        testBlob = (blob *)listGetStructOfNode(blobnode);

        //if the blob has resources and intersects the volume
        if ((testBlob->blobResources->numResources) && (volSphereIntersection(volume, testBlob->centre, testBlob->radius)))
        {
            //for each resource in the blob
            for (i=0; i < testBlob->blobResources->numResources; i++)
            {
                //if the resource is in the volume
                if (volPointInside(&volume, &testBlob->blobResources->ResourcePtr[i]->obj.posinfo.position))
                {
                    //add it to the temporary selection
                    selSelectionAddSingleShip((MaxSelection *)&tempSel, (ShipPtr)testBlob->blobResources->ResourcePtr[i]);
                }
            }
        }
        blobnode = blobnode->next;
    }

    if (tempSel.numTargets)
    {
        return (ResourceSelection *)selectMemDupSelection((SelectCommand *)&tempSel, "resinvol", 0);
    }
    else
    {
        return NULL;
    }
}


/*-----------------------------------------------------------------------------
    Name        : aiuShipIsResourcingOrSomething
    Description : Returns true if the ship is collecting resources
                  or docking (i.e. it's busy)
    Inputs      : ship - the resourcer
    Outputs     :
    Return      : TRUE if the resourcer is busy
----------------------------------------------------------------------------*/
bool32 aiuShipIsResourcingOrSomething(ShipPtr ship)
{
    return ((ship->rcstate1) ||
            (ship->rcstate2) ||
            (ship->dockvars.dockship) ||
            (ship->shipOrDerelict.catResourcing));     // CAT: Ship.catResourcing +0x1D8 (aiuShipIsResourcingOrSomething 0x0041dd50), HW1's SPECIAL_Resourcing
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindBestResource
    Description : Finds the best resource to collect for the ship -
                  the largest that will not overflow the ship's hold
    Inputs      : ship - the resourcer
    Outputs     :
    Return      : Pointer to the resource
----------------------------------------------------------------------------*/
Resource *aiuFindBestResource(Resource **biggestResource, ShipPtr ship, ResourceSelection *resources)
{
    udword ResourceRoom, i, biggest_resValue = 0; //, best_resValue = 0;
    real32 distsq, shortest_distsq = REALlyBig;
    Resource *best_resource = NULL;
    vector shippos = ship->obj.posinfo.position;

    ResourceRoom = ship->staticinfo->maxresources - ship->resources;

    for (i=0; i < resources->numResources;i++)
    {
        // CAT: a resource with the killer-collision attribute is never chosen (aiuFindBestResource 0x0041ddd7: attribute
        //      dword +0x38 & 0x200)
        if (resources->ResourcePtr[i]->obj.attributes & CAT_ATTRIBUTES_KillerCollision)
        {
            continue;
        }
        if ((distsq = aiuFindDistanceSquared(resources->ResourcePtr[i]->obj.posinfo.position, shippos)) < shortest_distsq)
        {
            shortest_distsq = distsq;
            best_resource = resources->ResourcePtr[i];
        }

//this code is to find the best fit resource for the ship
//        if ((resources->ResourcePtr[i]->resource.resourcevalue > best_resValue) &&
//            (resources->ResourcePtr[i]->resource.resourcevalue < ResourceRoom))
//        {
//            best_resource = resources->ResourcePtr[i];
//            best_resValue = best_resource->resource.resourcevalue;
//        }
        if (resources->ResourcePtr[i]->resourcevalue > biggest_resValue)
        {
            *biggestResource = resources->ResourcePtr[i];
            biggest_resValue = (*biggestResource)->resourcevalue;
        }
    }

    return best_resource;
}



/*-----------------------------------------------------------------------------
    Name        : aiuBlobInbetweenMothershipAndEnemyFactor
    Description : Returns a rating multiplier determined by whether the resource blob is in between the mothership and the enemy
    Inputs      : thisBlob - the blob being rated
    Outputs     :
    Return      : the modifying factor
----------------------------------------------------------------------------*/
real32 aiuBlobInbetweenMothershipAndEnemyFactor(blob *thisBlob)
{
    extern real32 RCONTROLLER_POS_FACTOR_INBETWEEN;
    extern real32 RCONTROLLER_POS_FACTOR_NOTINBETWEEN;
    Ship *mothership = aiCurrentAIPlayer->player->PlayerMothership;
    Ship *enemymothership;
    vector MotherToEnemy;
    vector MotherToBlob;
    vector EnemyToBlob;

    if (mothership == NULL)
    {
        return 1.0f;
    }

    enemymothership = aiuFindEnemyMothership(aiCurrentAIPlayer->player);

    if (enemymothership == NULL)
    {
        return 1.0f;
    }

    vecSub(MotherToEnemy,enemymothership->obj.posinfo.position,mothership->obj.posinfo.position);
    vecSub(MotherToBlob,thisBlob->centre,mothership->obj.posinfo.position);
    vecSub(EnemyToBlob,thisBlob->centre,enemymothership->obj.posinfo.position);

    if ( (vecDotProduct(MotherToBlob,MotherToEnemy) > 0) &&
         (vecDotProduct(EnemyToBlob,MotherToEnemy) < 0) )
    {
        return RCONTROLLER_POS_FACTOR_INBETWEEN;        // in between
    }
    else
    {
        return RCONTROLLER_POS_FACTOR_NOTINBETWEEN;     // not in between
    }
}


/*-----------------------------------------------------------------------------
    Name        : aiuRateBlobForResourcing
    Description : Rates a blob for how valuable it is for resourcing
    Inputs      : thisBlob - the blob being rated
    Outputs     :
    Return      : a rating of the blob depending on a whole bunch of stuff
----------------------------------------------------------------------------*/
real32 aiuRateBlobForResourcing(blob *thisBlob)
{
    extern real32 RCONTROLLER_POS_FACTOR_ENEMIESPRESENT;
    extern real32 RCONTROLLER_POS_FACTOR_ENEMIESPRESENT_BUT_OUTNUMBERED;
    extern real32 RCONTROLLER_POS_FACTOR_FRIENDLIES_PRESENT;
    sdword RUs = 0;
    sdword numEnemies = 0;
    sdword numFriendlies = 0;
    real32 factor = 1.0f;

    sdword i;
    ResourceSelection *blobResources = thisBlob->blobResources;
    SelectCommand *blobShips = thisBlob->blobShips;
    Ship *ship;

    for (i=0;i<blobResources->numResources;i++)
    {
        RUs += blobResources->ResourcePtr[i]->resourcevalue;
    }

    for (i=0;i<blobShips->numShips;i++)
    {
        ship = blobShips->ShipPtr[i];
        if (ship->playerowner == aiCurrentAIPlayer->player)
        {
            numFriendlies++;
        }
        else
        {
            numEnemies++;
        }
    }

    if (numEnemies > 0)
    {
        if (numEnemies > numFriendlies)
        {
            factor = RCONTROLLER_POS_FACTOR_ENEMIESPRESENT;
        }
        else
        {
            factor = RCONTROLLER_POS_FACTOR_ENEMIESPRESENT_BUT_OUTNUMBERED;
        }
    }
    else if (numFriendlies > 0)
    {
        factor = RCONTROLLER_POS_FACTOR_FRIENDLIES_PRESENT;
    }

    factor *= aiuBlobInbetweenMothershipAndEnemyFactor(thisBlob);

    return ((real32)RUs) * factor;
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindBestResourceBlob
    Description : Finds the best resource blob in the universe
    Inputs      : destination - call by reference location of the best resource blob
    Outputs     :
    Return      : TRUE if the best resource blob was found
----------------------------------------------------------------------------*/
bool32 aiuFindBestResourceBlob(vector *destination)
{
    Node *blobnode = universe.collBlobList.head;
    blob *thisBlob;
    blob *maxBlob = NULL;
    real32 rating;
    real32 maxrating = -1.0f;

    while (blobnode != NULL)
    {
        thisBlob = (blob *)listGetStructOfNode(blobnode);
        blobnode = blobnode->next;

        rating = aiuRateBlobForResourcing(thisBlob);
        if (rating > maxrating)
        {
            maxrating = rating;
            maxBlob = thisBlob;
        }
    }

    if (maxBlob)
    {
        *destination = maxBlob->centre;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindNearestResourceBlob
    Description : Finds the nearest resource blob to the selection of ships
    Inputs      : ships
    Outputs     :
    Return      : the Blob!
----------------------------------------------------------------------------*/
blob *aiuFindNearestResourceBlob(SelectCommand *ships)
{
    Node *blobnode = universe.collBlobList.head;
    blob *thisBlob;
    blob *closestBlob = NULL;
    real32 closestDistsq = REALlyBig, temp;
    vector shipscenter;

    shipscenter = selCentrePointComputeGeneral((MaxSelection *)ships, &temp);

    while (blobnode != NULL)
    {
        thisBlob = (blob *)listGetStructOfNode(blobnode);
        blobnode = blobnode->next;

        if ((thisBlob->blobResources->numResources) &&
            (temp = aiuFindDistanceSquared(thisBlob->centre, shipscenter)) < closestDistsq)
        {
            closestDistsq = temp;
            closestBlob   = thisBlob;
        }
    }

    return closestBlob;
}



/*-----------------------------------------------------------------------------
    Name        : aiuFindShipsAIBlob
    Description : Finds the blob the ships are in
    Inputs      : ships - the ships to check,
                  retentive - TRUE: function returns NULL if the ships aren't in the same blob
                              FALSE: returns the aiblob of the first ship in the selection
    Outputs     :
    Return      : The aiblob
----------------------------------------------------------------------------*/
struct aiblob *aiuFindShipsAIBlob(SelectCommand *ships, bool32 retentive)
{
    udword i;

    if (retentive)
    {
        for (i=0;i<aiuGoodGuyBlobs->numBlobs;i++)
        {
            if (TheseShipsAreInSelection(ships, aiuGoodGuyBlobs->blob[i]->blobShips))
            {
                return (struct aiblob *)aiuGoodGuyBlobs->blob[i];
            }
        }
        for (i=0;i<aiuEnemyBlobs->numBlobs;i++)
        {
            if (TheseShipsAreInSelection(ships, aiuEnemyBlobs->blob[i]->blobShips))
            {
                return (struct aiblob *)aiuEnemyBlobs->blob[i];
            }
        }
    }
    else
    {
        for (i=0;i<aiuGoodGuyBlobs->numBlobs;i++)
        {
            if (ShipInSelection(aiuGoodGuyBlobs->blob[i]->blobShips, ships->ShipPtr[0]))
            {
                return (struct aiblob *)aiuGoodGuyBlobs->blob[i];
            }
        }
        for (i=0;i<aiuEnemyBlobs->numBlobs;i++)
        {
            if (ShipInSelection(aiuEnemyBlobs->blob[i]->blobShips, ships->ShipPtr[0]))
            {
                return (struct aiblob *)aiuEnemyBlobs->blob[i];
            }
        }
    }

    return NULL;
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindShipAIBlob
    Description : Finds the blob the ship is in
    Inputs      : ship - the ship to check
    Outputs     :
    Return      : The aiblob
----------------------------------------------------------------------------*/
struct aiblob *aiuFindShipAIBlob(ShipPtr ship)
{
    udword i;

    if (ship->playerowner == aiCurrentAIPlayer->player)
    {
        for (i=0;i<aiuGoodGuyBlobs->numBlobs;i++)
        {
            if (ShipInSelection(aiuGoodGuyBlobs->blob[i]->blobShips, ship))
            {
                return (struct aiblob *)aiuGoodGuyBlobs->blob[i];
            }
        }
    }
    else
    {
        for (i=0;i<aiuEnemyBlobs->numBlobs;i++)
        {
            if (ShipInSelection(aiuEnemyBlobs->blob[i]->blobShips, ship))
            {
                return (struct aiblob *)aiuEnemyBlobs->blob[i];
            }
        }
    }
    return NULL;
}


/*-----------------------------------------------------------------------------
    Name        : aiuEnemyShipIsVisible
    Description : Finds out if the enemy ship is visible
    Inputs      : ship - the enemy ship
    Outputs     :
    Return      : TRUE if the enemy ship is visible
----------------------------------------------------------------------------*/
bool32 aiuEnemyShipIsVisible(ShipPtr ship)
{
    aiblob *shipblob;

    if (!aiuShipIsntTargetable(ship, aiCurrentAIPlayer->player))
    {
        shipblob = (aiblob *)aiuFindShipAIBlob(ship);
        return shipblob->visibility;
    }

    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindMinimumGunRangeSquared
    Description : Finds the minimum gun ranges of all the ships in the selection
                  Note: This function scales the minimum gun ranges to work better
                        with the move attack command
    Inputs      : ships - the ships to find the minimum gun ranges of
    Outputs     :
    Return      : the minimum gun range in the group
----------------------------------------------------------------------------*/
real32 aiuFindMinimumGunRangeSquared(SelectCommand *ships)
{
    real32 minrange = REALlyBig;
    udword i;
    ShipPtr ship;

    for (i=0;i<ships->numShips;i++)
    {
        ship = ships->ShipPtr[i];

        if (!isCapitalShip(ship))
        {
            //don't do this check
            continue;
        }

        if (minrange > ship->staticinfo->bulletRangeSquared[ship->tacticstype])
        {
            minrange = ship->staticinfo->bulletRangeSquared[ship->tacticstype];

            //reduce the minrange even more
            // CAT: tMissileDestroyer, sDreadnought and sDestroyer (aiuFindMinimumGunRangeSquared 0x0041e2d0: 0x55 0x0a 0x19)
            if ((ship->shiptype == tMissileDestroyer) || (ship->shiptype == sDreadnought) || (ship->shiptype == sDestroyer))
            {
                minrange *= frandyrandombetween(RANDOM_AI_PLAYER, 0.10f, 0.25f);
            }
            else
            {
                minrange *= frandyrandombetween(RANDOM_AI_PLAYER, 0.20f, 0.64f);
            }
        }
    }
    dbgAssertOrIgnore(ships->numShips > 0);
    dbgAssertOrIgnore(minrange < REALlyBig);

    return minrange;
}

/*-----------------------------------------------------------------------------
    Name        : aiuShipsInGunRange
    Description : Returns TRUE if ships of selection1 are in gun range of its attack targets
                  Note: this function, assumes all the ships in selection 1 have an
                        attack command
    Inputs      : selection1
    Outputs     :
    Return      : TRUE if the ships are in gun range
----------------------------------------------------------------------------*/
bool32 aiuShipsInGunRangeOfTargets(SelectCommand *selection)
{
//    real32 minrange, dummy, distSqr;
//    vector midselection1, distvec;
//    udword i;

//    minrange = RangeToTarget(selection1->ShipPtr[0], (SpaceObjRotImpTarg *)selection2->ShipPtr[0], selection2->ShipPtr[0]->obj.posinfo.position);
//    vecSub(distvec,selection1->ShipPtr[0]
//    minrange = aiuFindMinimumGunRangeSquared(selection1);
//    midselection1 = selCentrePointComputeGeneral((MaxSelection *)selection1, &dummy);

//    for (i=0;i<selection2->numShips;i++)
//    {
//        vecSub(distvec,midselection1,selection2->ShipPtr[i]->obj.posinfo.position);
//        distSqr = vecMagnitudeSquared(distvec);

//        if (distSqr*0.90 < minrange)  //may need to tweak this a little
//        {
//            return TRUE;
//        }
//    }
//    return FALSE;

    sdword i,j;
    vector distvec;
    real32 distSqr;
    sdword breaker;
    ShipPtr ship;
    CommandToDo *command;

    for(i=0;i<selection->numShips;i++)
    {
        ship = selection->ShipPtr[i];

        // CAT: both races' cloak and grav well generators and DFG frigates
        //      (aiuShipsInGunRangeOfTargets 0x0041e3a0: 0x3f 0x4d 0x40 0x4f 0x4e 0x2d)
        if(ship->shiptype == kCloakGenerator || ship->shiptype == tCloakGenerator ||
           ship->shiptype == kGravWellGenerator || ship->shiptype == tGravWellGenerator ||
           ship->shiptype == tDFGFrigate || ship->shiptype == bDFGFrigate)
            continue;
        command = getShipAndItsCommand(&universe.mainCommandLayer,ship);

        //only calculate this stuff for attacking ships
        if (!command || command->ordertype.order != COMMAND_ATTACK)
            continue;
//        dbgAssertOrIgnore(command != NULL);
//        dbgAssertOrIgnore(command->ordertype.order == COMMAND_ATTACK);

        breaker = FALSE;
        for(j = 0; j<command->attack->numTargets;j++)
        {
//            vecSub(distvec,ship->obj.posinfo.position,command->attack->TargetPtr[j]->obj.posinfo.position);
//            distSqr = vecMagnitudeSquared(distvec);
            vecSub(distvec, command->attack->TargetPtr[j]->obj.posinfo.position, ship->obj.posinfo.position);
            distSqr = RangeToTarget(ship, command->attack->TargetPtr[j], &distvec);
            distSqr *= distSqr;

            if(distSqr < ship->staticinfo->bulletRangeSquared[ship->tacticstype])
            {
                //within range...break and check next ship
                breaker = TRUE;
                break;
            }
        }
        if(!breaker)
            return FALSE;   //ship will not be within range
    }
    return TRUE;
}

/*=============================================================================
    Unit Cap Checking:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : aiuCheckRequestQueue
    Description : Adds any ships in the request queue
    Inputs      : aiplayer - the aiplayer to check
                  node - the first node of the queue to check,
                  shipclass - the class of ship to check for,
                  shiptype - the type of ship to check for,
                  shiptotal - call by reference total number of ships for the CPU,
                  classtotal - call by reference total of that class for the cPU,
                  typetotal - call by reference total of that type for the CPU
    Outputs     : modifies shiptotal, classtotal and typetotal
    Return      : void
----------------------------------------------------------------------------*/
void aiuCheckRequestQueue(AIPlayer *aiplayer, Node *node,
                          ShipClass shipclass, ShipType shiptype,
                          sdword *shiptotal, sdword *classtotal, sdword *typetotal)
{
    ShipStaticInfo *tempstat;
    RequestShips *request;

    while (node != NULL)
    {
        request = (RequestShips *)listGetStructOfNode(node);

        *shiptotal += request->num_ships;

        tempstat = GetShipStaticInfo(request->shiptype, aiplayer->player->race);
        if (tempstat->shipclass == shipclass)
        {
            *classtotal += request->num_ships;

            if (request->shiptype == shiptype)
            {
                *typetotal += request->num_ships;
            }
        }
        node = node->next;
    }
}



/*-----------------------------------------------------------------------------
    Name        : aiuUnitCapCanBuildShip
    Description : Uses the computer player's own unit caps scheme because the one
                  the game uses is stupid
    Inputs      : aiplayer - the aiplayer checking if it has exceeded the ship caps,
                  shiptype - the shiptype being requested,
                  numShips - the number of ships being requested
    Outputs     :
    Return      : the number of ships the player can build
----------------------------------------------------------------------------*/
extern sdword cdMaxShipsAllowed;
extern sdword cdLimitCaps[TOTAL_NUM_SHIPS];
extern sdword cdClassCaps[NUM_CLASSES];
extern bool32   cdEnabled;
udword aiuUnitCapCanBuildShip(AIPlayer *aiplayer, ShipType shiptype, sdword numShips)
{
    sdword shiptotal = 0, classtotal = 0, typetotal = 0, shipcount = 0;
    udword i;
    ShipStaticInfo *requeststat, *tempstat;
    ShipClass shipclass;
    Node *node;
    Player *player = aiplayer->player;

    if ((!cdEnabled) || (singlePlayerGame))
    {
        return numShips;
    }

    requeststat = GetShipStaticInfo(shiptype, aiplayer->player->race);
    shipclass   = requeststat->shipclass;

    // go through the CPU's own build queues
    // CAT: over the player's race's ship types, FirstShipTypeOfRace to LastShipTypeOfRace (the last one excluded, as the
    //      exe's loop ends there: aiuUnitCapCanBuildShip 0x0041e500), where HW1 went over every standard type
    for (i=FirstShipTypeOfRace[aiplayer->player->race];i<LastShipTypeOfRace[aiplayer->player->race];i++,shipcount=0)
    {
        tempstat = GetShipStaticInfo(i, aiplayer->player->race);

        //count how many of this ship is being built by the AI
        shipcount += aiplayer->AttackManShipsBeingBuilt.NumShipsOfType[i];
        shipcount += aiplayer->DefenseManShipsBeingBuilt.NumShipsOfType[i];
        shipcount += aiplayer->ScriptManShipsBeingBuilt.NumShipsOfType[i];

        //add to the total number of ships
        shiptotal += shipcount;

        if (tempstat->shipclass == shipclass)
        {
            //add to the total number of that class
            classtotal += shipcount;

            if (i==shiptype)
            {
                //add to the total number of that type
                typetotal += shipcount;
            }
        }
    }

    // go through all the request ships queues
    node = aiplayer->AttackManRequestShipsQ.head;
    aiuCheckRequestQueue(aiplayer, node, shipclass, shiptype, &shiptotal, &classtotal, &typetotal);

    node = aiplayer->DefenseManRequestShipsQ.head;
    aiuCheckRequestQueue(aiplayer, node, shipclass, shiptype, &shiptotal, &classtotal, &typetotal);

    node = aiplayer->ScriptManRequestShipsQ.head;
    aiuCheckRequestQueue(aiplayer, node, shipclass, shiptype, &shiptotal, &classtotal, &typetotal);

    // add to that the ships in the newships structure
    for (i=0;i<aiplayer->newships.selection->numShips;i++)
    {
        shiptotal++;
        if (aiplayer->newships.selection->ShipPtr[i]->staticinfo->shipclass == shipclass)
        {
            classtotal++;
            if (aiplayer->newships.selection->ShipPtr[i]->shiptype == shiptype)
            {
                typetotal++;
            }
        }
    }

    shiptotal += aiplayer->player->totalships;

    //more additions of ships being built because resource manager is
    //fucked up and doesn't have a ResourceManShipsBeingBuilt structure
    shiptotal += aiplayer->NumRCollectorsBeingBuilt;
    shiptotal += aiplayer->NumRControllersBeingBuilt;
    shiptotal += aiplayer->NumASFBeingBuilt;
    shiptotal += aiplayer->NumResearchShipsBeingBuilt;

    // CAT: the resource collectors are the Workers and the Taiidan collector, the controllers the Processors, the
    //      support frigates both races' (aiuUnitCapCanBuildShip 0x0041e71c switch: 0x0e 0x31 0x58 / 0x10 0x33 / 0x3c 0x4a); the
    //      HW1 ResourceController and ResearchShip cases are gone with those types
    switch (shiptype)
    {
        case sWorker:
        case bWorker:
        case tResourceCollector:
            typetotal  += aiplayer->NumRCollectorsBeingBuilt;
            classtotal += aiplayer->NumRCollectorsBeingBuilt;
            classtotal += aiplayer->NumRControllersBeingBuilt;
            break;
        case sProcessor:
        case bProcessor:
            typetotal  += aiplayer->NumRControllersBeingBuilt;
            classtotal += aiplayer->NumRControllersBeingBuilt;
            classtotal += aiplayer->NumRCollectorsBeingBuilt;
            break;
        case kAdvanceSupportFrigate:
        case tAdvanceSupportFrigate:
            typetotal  += aiplayer->NumASFBeingBuilt;
            classtotal += aiplayer->NumASFBeingBuilt;
            break;
        default:
            switch (shipclass)
            {
                case CLASS_Resource:
                    classtotal += aiplayer->NumRCollectorsBeingBuilt;
                    classtotal += aiplayer->NumRControllersBeingBuilt;
                    break;
                case CLASS_Frigate:
                    classtotal += aiplayer->NumASFBeingBuilt;
                    break;
                case CLASS_NonCombat:
                    classtotal += aiplayer->NumResearchShipsBeingBuilt;
                    break;
            }
            break;
    }

    if (cdMaxShipsAllowed - shiptotal < numShips)
    {
        if (cdMaxShipsAllowed - shiptotal < 0)
        {
            return 0;
        }
        else
        {
            return (cdMaxShipsAllowed - shiptotal);
        }
    }

    if (cdLimitCaps[shiptype] != -1)
    {
        if (cdLimitCaps[shiptype] - (player->shiptotals[shiptype]+typetotal) < numShips)
        {
            if (cdLimitCaps[shiptype] - (player->shiptotals[shiptype]+typetotal) < 0)
            {
                return 0;
            }
            else
            {
                return (cdLimitCaps[shiptype] - (player->shiptotals[shiptype]+typetotal));
            }
        }
    }

    // CAT: a class cap of -1 is no cap, as for the type caps (0x0041e7bf: cdClassCaps 0x00b3aba0 compared with -1)
    if (cdClassCaps[shipclass] != -1)
    {
        if (cdClassCaps[shipclass] - (player->classtotals[shipclass]+classtotal) < numShips)
        {
            if (cdClassCaps[shipclass] - (player->classtotals[shipclass]+classtotal) < 0)
            {
                return 0;
            }
            else
            {
                return (cdClassCaps[shipclass] - (player->classtotals[shipclass]+classtotal));
            }
        }
    }

    return numShips;
}


///extra code
    // now go through the command layer build commands
//  node = universe.mainCommandLayer.todolist.head;
//  while (node != NULL)
//  {
//      command = (CommandToDo *)listGetStructOfNode(node);
//
//      if ((command->ordertype.order == COMMAND_BUILDING_SHIP) &&
//          (command->buildingship.playerIndex == aiIndex))
//      {
//          shiptotal++;
//
//          tempstat = GetShipStaticInfo(command->buildingship.shipType, aiplayer->player->race);
//          if (tempstat->shipclass == shipclass)
//          {
//              classtotal++;
//              if (command->buildingship.shipType == shiptype)
//              {
//                  typetotal++;
//              }
//          }
//      }
//      node = node->next;
//  }






/*=============================================================================
    Armada Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : aiuFindArmadaTarget
    Description : Finds a target for an armada attack
    Inputs      : dest_target - pass by reference variable to be filled with the
                                general location of the target
                  sel_target - selection of the targets
                  team_sel   - the team's ships
    Outputs     : Finds a target to have it's ass kicked
    Return      : Whether the target is visible or not
----------------------------------------------------------------------------*/
bool32 aiuFindArmadaTarget(vector *dest_target, MaxSelection *sel_target, SelectCommand *team_sel)
{
    aiblob *blob, *best_blob = NULL, *mothership_blob = NULL;
    udword i/*, best_num_ships*/;
    real32 primary_value_strength_ratio = 0.0, best_primary_value_strength_ratio = 0.0, mothership_value_strength_ratio = 0.0;
    real32 randvar = 0.0;

    //NOTE: later may need to consider strength of attack team.  In addition, may need to
    //      change criteria for choosing blobs - not only high value/strength, but also
    //      occasionally attack high strength groups, regardless of value, because we want
    //      to reduce risk to CP's mothership

    //rate all non-mothership enemy blobs
    for (i=0;i<aiuEnemyBlobs->numBlobs;i++)
    {
        blob = aiuEnemyBlobs->blob[i];

        if (blob->mothership)
        {
            //if it is a primary enemy mothership blob
            //maybe subtract the mothership value from the blob
            //could possibly run around the main attack force and attack the mothership behind it
            //or just take out the main attack force.  Ooooh!!! Decision making time
            if (blob->primaryenemyvalue > AIU_MOTHERSHIP_VALUE)
            {
                mothership_value_strength_ratio = (((real32)blob->primaryenemyvalue*(500.0))/(real32)blob->primaryenemystrength);
                mothership_blob = blob;
                if (aiuEnemyBlobs->numBlobs == 1)
                {
                    best_blob = blob;
                }
                continue;
            }
        }
        else if (blob->primaryenemystrength)
        {
            primary_value_strength_ratio = (((real32)blob->primaryenemyvalue*(500.0))/(real32)blob->primaryenemystrength);
        }
        else
        {
            primary_value_strength_ratio = (real32)(blob->primaryenemyvalue);
        }

        //add error value to strength ratio
        if (!blob->visibility)
        {
            //medium error with sensor array
            if (1)//aiCurrentAIPlayer->SensorArray)
            {
                randvar = frandyrandombetween(RANDOM_AI_PLAYER, -primary_value_strength_ratio/3,
                                              primary_value_strength_ratio/3);
            }
            //high error with no sensor array
            else
            {
                randvar = frandyrandombetween(RANDOM_AI_PLAYER, -primary_value_strength_ratio/1.2,
                                              primary_value_strength_ratio/1.2);
            }
            primary_value_strength_ratio += randvar;
            dbgAssertOrIgnore(primary_value_strength_ratio >= 0);
        }

        //the best blob stays.  Yay!
        if (primary_value_strength_ratio > best_primary_value_strength_ratio)
        {
            best_primary_value_strength_ratio = primary_value_strength_ratio;
            best_blob = blob;
        }
    }

    if (best_primary_value_strength_ratio*1.5 > mothership_value_strength_ratio)
    {
        *dest_target  = best_blob->centre;
        // CAT: copied into the AIPlayer's own selection (cat_selSelectionCopy at 0x0041e9d4; M2-R7 ai-player-skirmish)
        cat_selSelectionCopy(sel_target, (MaxSelection *)best_blob->blobShips);
        MakeTargetsOnlyEnemyShips((SelectAnyCommand *)sel_target, aiCurrentAIPlayer->player);
        return best_blob->visibility;
    }
    // CAT: the mothership fallback needs a live mothership and its blob, and targets the mothership alone (HW1's
    //      "postfinal" ships within 9000 are gone): aiuFindArmadaTarget 0x0041e860 from 0x0041e9fa.
    //      The selection is the one the exe keeps in AIPlayer (ArmadaType at +0xCF4: targets +0xCF8; M2-R7
    //      ai-player-skirmish embedded it).
    else if ((aiCurrentAIPlayer->primaryEnemyPlayer->PlayerMothership != NULL) &&
             (!bitTest(aiCurrentAIPlayer->primaryEnemyPlayer->PlayerMothership->header.flags, SOF_Dead)) &&
             (mothership_blob != NULL))
    {
        *dest_target = mothership_blob->centre;
        sel_target->numShips = 1;
        sel_target->ShipPtr[0] = aiCurrentAIPlayer->primaryEnemyPlayer->PlayerMothership;
        return mothership_blob->visibility;
    }
    else
    {
        aiplayerLog((aiIndex,"Error - couldn't find primary enemy player's mothership"));
        sel_target->numShips = 0;
        return FALSE;
    }
}



/*=============================================================================
    AI Wrap Functions:
=============================================================================*/
#define AIU_ATTACKSINGLE_STRENGTH   1.5
/*-----------------------------------------------------------------------------
    Name        : aiuSetAttackSingleTarget
    Description : Sets the attack command for when the target is simply a single target
    Inputs      : attackers - the attackers
                  target    - the target
    Outputs     : Makes ships attack the single solitary little girly ship
    Return      : void
----------------------------------------------------------------------------*/
void aiuSetAttackSingleTarget(SelectCommand *attackers, SelectCommand *target)
{
    aiuWrapAttack(attackers, target);
}


// CAT (M2-R7 oracle-rung-aiship): no aiuFindPriorityTarget (HW1 1.05 "postfinal"): cataclysm.exe has no such function
//      and its aiuAttack 0x0041ea60 attacks the targets as given (aiuSetAttackHomogenous); the HW1 body drew from
//      stream 6 on every homogenous attack

/*-----------------------------------------------------------------------------
    Name        : aiuSetAttackHomogenous
    Description : Sets the attacks for a homogenous selection
    Inputs      : attackers - the attackers
                  targets   - the attackees
    Outputs     : Makes ships attack and stuff
    Return      : void
----------------------------------------------------------------------------*/
void aiuSetAttackHomogenous(SelectCommand *attackers, SelectCommand *targets)
{
//    SelectCommand *att_target;

    // CAT: no aiuFindPriorityTarget (cataclysm.exe has none: aiuAttack 0x0041ebf3 inlines this as "one target: attack;
    //      then attack", without HW1's random target preference on stream 6)
    if (targets->numShips == 1)
    {
        aiuSetAttackSingleTarget(attackers, targets);
    }
    aiuWrapAttack(attackers, targets);
}


/*-----------------------------------------------------------------------------
    Name        : aiuSetAttackHetrogenous
    Description : Sets the attacks for a hetrogenous selection of attackers
    Inputs      : attackers - the attackers
                  targets   - the attackees
    Outputs     : Makes ships attack and stuff
    Return      : void
----------------------------------------------------------------------------*/
void aiuSetAttackHeterogenous(SelectCommand *attackers, SelectCommand *targets)
{
    udword i;
    ShipType att_type;
    SelectCommand *HomoAttackers = (SelectCommand *)memAlloc(sizeofSelectCommand(attackers->numShips), "aiuathet", 0);

    while (attackers->numShips != 0)
    {
        att_type = attackers->ShipPtr[0]->shiptype;
        aiuMoveShipSelection((MaxSelection *)HomoAttackers, (MaxSelection *)attackers, 0);

        for (i=0;i<attackers->numShips;)
        {
            if (attackers->ShipPtr[i]->shiptype == att_type)
            {
                aiuMoveShipSelection((MaxSelection *)HomoAttackers, (MaxSelection *)attackers, i);
            }
            else
            {
                i++;
            }
        }
        aiuSetAttackHomogenous(HomoAttackers, targets);
        HomoAttackers->numShips = 0;
    }

    memFree(HomoAttackers);
}



/*-----------------------------------------------------------------------------
    Name        : aiuWrapAttack
    Description : Takes out non-attacking ships out of a team, then, if the
                  difficulty level of the computer player is high enough, the
                  ships get assigned to their best target
    Inputs      : team - the attacking team
                  selection - the attackees
    Outputs     : Makes ships attack other ships
    Return      : Selection of ships that have become targets
----------------------------------------------------------------------------*/
SelectCommand *aiuAttack(AITeam *team, SelectCommand *targets)
{
    MaxSelection Attackers;
    SelectCommand *nonAttackers;
    SelectCommand *AttTargets = (SelectCommand *)selectMemDupSelection(targets, "aiuatt", 0);
    sdword numNonAttacking = 0;

    //later add feature enabled (micromanage chose attack) check
    //change "aimProcessAttack" to use aiuAttack instead of aiuWrapAttack
    if (team->teamType == ScriptTeam)
    {
        aiuWrapAttack(team->shipList.selection, AttTargets);
        goto finish;
    }

    //take out non-attacking ships
    MakeShipsAttackCapable((SelectCommand *)&Attackers, team->shipList.selection, FALSE);   // CAT: skip-disabled argument (aiuAttack 0x0041ea60 passes 0)
    numNonAttacking = team->shipList.selection->numShips - Attackers.numShips;

    if (!Attackers.numShips)
    {
        goto finish;
    }

    if (numNonAttacking)
    {
        nonAttackers = selectMemDupSelection(team->shipList.selection, "auap", Pyrophoric);
        MakeShipsNotIncludeTheseShips(nonAttackers, (SelectCommand *)&Attackers);

        //non-Attackers guard attacking ships
        aiuWrapProtect(nonAttackers, (SelectCommand *)&Attackers);
        memFree(nonAttackers);
    }

    if (aitTeamHomogenous(team))
    {
        aiuSetAttackHomogenous((SelectCommand *)&Attackers, AttTargets);
    }
    else
    {
        aiuSetAttackHeterogenous((SelectCommand *)&Attackers, AttTargets);
    }

  finish:
    // CAT: the duplicate is returned and kept (aiuAttack 0x0041ea60 returns it, aimProcessAdvancedAttack 0x00408ca0 stores
    //      it and frees the original): aiuWrapAttack drops the untargetable targets from it in place. HW1's postfinal
    //      freed the duplicate and returned the original
    return AttTargets;
}


/*-----------------------------------------------------------------------------
    Name        : aiuSplitAttack
    Description : Splits the attackers into smaller teams to attack individual targets
                  (recommended only for strike craft attackers and targets)
                  Note: assumes that the odds have been stacked against the targets
    Inputs      : ships - the attackers
                  targets - the attackees
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aiuSplitAttack(SelectCommand *ships, SelectCommand *targets)
{
    MaxSelection  split;
    SelectCommand selone;
    udword i, j, split_num;

    split_num       = ships->numShips/targets->numShips;
    selone.numShips = 1;

    for (i=0,j=0;(i<targets->numShips)&&(j<ships->numShips);i++)
    {
        udword att_index  = ((i+1)*split_num);
        selone.ShipPtr[0] = targets->ShipPtr[i];
        split.numShips    = 0;
        for (;(j<att_index)&&(j<ships->numShips);j++)
        {
            selSelectionAddSingleShip(&split, ships->ShipPtr[j]);
        }

        if ((i+1)==targets->numShips)
        {
            for (;j<ships->numShips;j++)
            {
                selSelectionAddSingleShip(&split, ships->ShipPtr[j]);
            }
        }
        aiuWrapAttack((SelectCommand *)&split, &selone);
    }
}



// CAT (M2-R7 ai-orders-moves): no aiuSwarmDock (P2 swarm moves only; not in cataclysm.exe)


/*-----------------------------------------------------------------------------
    Name        : aiuWrap*
    Description : The following functions wrap the main commands for the computer
                  player.  Certain checks are done before the command is issued.
    Inputs      : Depends on command
    Outputs     : issues commands
    Return      : void
----------------------------------------------------------------------------*/


bool32 aiuWrapAttack(SelectCommand *attackers, SelectCommand *targets)
{
    MaxSelection filtered;
    MaxSelection leeches;
    bool32 issued = FALSE;
    sdword i;

    filtered.numShips = 0;
    // CAT: aiuWrapAttack 0x0041ed00: the untargetable targets are dropped from the list in place (where HW1 refused the
    //      whole attack when one target was untargetable), and every sLeech of the attackers is taken out of the attack:
    //      those not already leeching get a special-target order on the targets (cat_leechIsLeeching 0x005b7ce0,
    //      clWrapSpecial), the rest attack
    if (aiuFilterSelectableShips(attackers, &filtered) &&
        (cat_aiuRemoveUntargetableTargets(targets) != 0))
    {
        leeches.numShips = 0;
        for (i = 0; i < attackers->numShips; i++)
        {
            if (attackers->ShipPtr[i]->shiptype == sLeech)
            {
                clRemoveShipFromSelection((SelectCommand *)&filtered, attackers->ShipPtr[i]);
                if (!cat_leechIsLeeching(attackers->ShipPtr[i]))
                {
                    leeches.ShipPtr[leeches.numShips++] = attackers->ShipPtr[i];
                }
            }
        }
        if (leeches.numShips > 0)
        {
            clWrapSpecial(&universe.mainCommandLayer, (SelectCommand *)&leeches, (SpecialCommand *)targets);
            issued = TRUE;
        }
        if (filtered.numShips > 0)
        {
#ifdef HW_BUILD_FOR_DEBUGGING
            if (attackers->ShipPtr[0]->playerowner == targets->ShipPtr[0]->playerowner)
            {
                aiplayerLog((aiIndex, "Heap Big Warning!  CPU attacking itself!"));
            }
#endif
            clWrapAttack(&universe.mainCommandLayer,(SelectCommand *)&filtered, (AttackCommand *)targets);
            return TRUE;
        }
    }
    return issued;
}


bool32 aiuWrapMove(SelectCommand *ships, vector destination)
{
    MaxSelection filtered;

    filtered.numShips = 0;

    if (aiuFilterSelectableShips(ships, &filtered))
    {
        // CAT: the ships are made mobile first (MakeShipsMobile, aiuWrapMove 0x0041ef17), then a one-point path from the
        //      first ship (aiuWrapMove 0x0041eea0)
        MakeShipsMobile((SelectCommand *)&filtered, FALSE);
        clWrapMove(&universe.mainCommandLayer,(SelectCommand *)&filtered,CAT_MOVEPATH_ONCE,ships->ShipPtr[0]->obj.posinfo.position,1,&destination);
        return TRUE;
    }
    return FALSE;
}

bool32 aiuWrapDock(SelectCommand *ships, DockType docktype, ShipPtr dockwith)
{
    MaxSelection filtered;

    filtered.numShips = 0;

    if (dockwith)
    {
        if (aiuFilterSelectableShips(ships, &filtered) &&
            !aiuShipIsntSelectable(dockwith))
        {
            clWrapDock(&universe.mainCommandLayer,(SelectCommand *)&filtered, docktype, dockwith);
            return TRUE;
        }
    }
    else
    {
        if (aiuFilterSelectableShips(ships, &filtered))
        {
            clWrapDock(&universe.mainCommandLayer,(SelectCommand *)&filtered, docktype, dockwith);
            return TRUE;
        }
    }
    return FALSE;
}

bool32 aiuKasWrapFormation(SelectCommand *ships, TypeOfFormation formation)
{
    MaxSelection filtered;

    filtered.numShips = 0;

    if (aiuFilterDisabledShips(ships, &filtered))
    {
        clWrapFormation(&universe.mainCommandLayer,(SelectCommand *)&filtered, formation, 0);   // CAT: level 0 (aiuWrapFormation 0x0041f0b0)
        return TRUE;
    }
    return FALSE;
}

bool32 aiuWrapFormation(SelectCommand *ships, TypeOfFormation formation)
{
    MaxSelection filtered;

    filtered.numShips = 0;

    if (aiuFilterSelectableShips(ships, &filtered))
    {
        clWrapFormation(&universe.mainCommandLayer,(SelectCommand *)&filtered, formation, 0);   // CAT: level 0 (aiuWrapFormation 0x0041f0b0)
        return TRUE;
    }
    return FALSE;
}

bool32 aiuWrapProtect(SelectCommand *ships, SelectCommand *shipstoguard)
{
    MaxSelection filtered, guardfiltered;

    filtered.numShips      = 0;
    guardfiltered.numShips = 0;

    if (aiuFilterSelectableShips(ships, &filtered) &&
        aiuFilterSelectableShips(shipstoguard, &guardfiltered))
    {
        clWrapProtect(&universe.mainCommandLayer,(SelectCommand *)&filtered, (ProtectCommand *)&guardfiltered);
        return TRUE;
    }
    return FALSE;
}


bool32 aiuWrapSpecial(SelectCommand *ships, SelectCommand *targets)
{
    MaxSelection filtered;

    filtered.numShips = 0;
    // CAT: the untargetable targets are dropped from the list in place, and the order goes only when targets are left
    //      (aiuWrapSpecial 0x0041f2e0); HW1 refused it when one target was untargetable
    if (aiuFilterSelectableShips(ships, &filtered) &&
        ((!targets) || (cat_aiuRemoveUntargetableTargets(targets) != 0)))
    {
        clWrapSpecial(&universe.mainCommandLayer,(SelectCommand *)&filtered, (SpecialCommand *)targets);
        return TRUE;
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_aiuWrapCapture
    Description : CAT: new (0x0041f3d0): the computer player's capture order, a special-target order with no targets for
                  the selectable ships of the selection (cat_clWrapSpecialTarget 0x00453130 with flag), unless dontIssue
    Inputs      : ships, dontIssue, flag
    Outputs     :
    Return      : TRUE if the order was given
----------------------------------------------------------------------------*/
bool32 cat_aiuWrapCapture(SelectCommand *ships, bool32 dontIssue, bool32 flag)
{
    MaxSelection filtered;

    filtered.numShips = 0;
    if (aiuFilterSelectableShips(ships, &filtered) && (!dontIssue))
    {
        cat_clWrapSpecialTarget(&universe.mainCommandLayer, (SelectCommand *)&filtered, NULL, flag);
        return TRUE;
    }
    return FALSE;
}


bool32 aiuWrapCollectResource(SelectCommand *ships, ResourcePtr resource)
{
    MaxSelection filtered;

    filtered.numShips = 0;

    if (aiuFilterSelectableShips(ships, &filtered))
    {
        clWrapCollectResource(&universe.mainCommandLayer,(SelectCommand *)&filtered, resource);
        return TRUE;
    }
    return FALSE;
}

bool32 aiuWrapSetTactics(SelectCommand *ships, TacticsType tactics)
{
    MaxSelection filtered;

    filtered.numShips = 0;

    if (aiuFilterSelectableShips(ships, &filtered))
    {
        clWrapSetTactics(&universe.mainCommandLayer,(SelectCommand *)&filtered, tactics);
        return TRUE;
    }
    return FALSE;
}

bool32 aiuWrapSetKamikaze(SelectCommand *ships)
{
    MaxSelection filtered;

    filtered.numShips = 0;

    if (aiuFilterSelectableShips(ships, &filtered))
    {
        clWrapSetKamikaze(&universe.mainCommandLayer,(SelectCommand *)&filtered);
        return TRUE;
    }
    return FALSE;
}

bool32 aiuWrapHalt(SelectCommand *ships)
{
    MaxSelection filtered;

    filtered.numShips = 0;

    if (aiuFilterSelectableShips(ships, &filtered))
    {
        clWrapHalt(&universe.mainCommandLayer,(SelectCommand *)&filtered);
        return TRUE;
    }
    return FALSE;
}

bool32 aiuWrapScuttle(SelectCommand *ships)
{
    MaxSelection filtered;

    filtered.numShips = 0;

    if (aiuFilterSelectableShips(ships, &filtered))
    {
        clWrapScuttle(&universe.mainCommandLayer,(SelectCommand *)&filtered);
        return TRUE;
    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aiuWrapGetCollBlob
    Description : Gets the collision blob of a selection of ships.
                  Note: assumes that the selection is in one blob
    Inputs      : ships - the selection of ships to get the coll blob from
    Outputs     :
    Return      : The collision blob of the ships, NULL if the have no collblob
----------------------------------------------------------------------------*/
blob *aiuWrapGetCollBlob(SelectCommand *ships)
{
    blob *collBlob = NULL;
    udword i;

    for (i=0; i<ships->numShips;i++)
    {
        collBlob = ships->ShipPtr[i]->obj.collMyBlob;

        if (collBlob)
        {
            return collBlob;
        }
    }
    return collBlob;
}



/*=============================================================================
    Primary Enemy Utility Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : aiuChangePrimaryEnemy
    Description : Changes all the general primary enemy stuff in the aiplayer struct
    Inputs      : aiplayer - the player who's primary enemy has changed
    Outputs     : Changes a few ship selections and stuff
    Return      : void
----------------------------------------------------------------------------*/
void aiuChangePrimaryEnemy(AIPlayer *aiplayer)
{
    //assumes that the new primary enemy player has been chosen
    Player *primaryEnemy = aiplayer->primaryEnemyPlayer;
    udword i,j;
    ShipPtr ship;

    for (i=0; i<TOTAL_NUM_SHIPS;i++)
    {
        //reset all the primary enemy ships I am aware of
        aiplayer->primaryEnemyShipsIAmAwareOf[i].selection->numShips = 0;

        //move any ships that are now the new primary enemies into the
        //primary enemy ships I am aware of structure
        for (j=0;j<aiplayer->enemyShipsIAmAwareOf[i].selection->numShips;)
        {
            ship = aiplayer->enemyShipsIAmAwareOf[i].selection->ShipPtr[j];
            if (ship->playerowner == primaryEnemy)
            {
                growSelectRemoveShipIndex(&aiplayer->enemyShipsIAmAwareOf[i], j);
                growSelectAddShip(&aiplayer->primaryEnemyShipsIAmAwareOf[i], ship);
            }
            else
            {
                j++;
            }
        }
    }
}


/*-----------------------------------------------------------------------------
    Name        : aiuFindCloakersInEnemyShipsIAmAwareOf
    Description : The name says it all...
    Inputs      : Red_Alert - whether a red alert status can be returned
    Outputs     : May change the aicurrentAIPlayer->alertflags
    Return      : Whether the status has changed
----------------------------------------------------------------------------*/
bool32 aiuFindCloakersInEnemyShipsIAmAwareOf(bool32 Red_Alert)
{
    bool32 change = FALSE;

    if (bitTest(aiCurrentAIPlayer->AlertStatus, ALERT_CLOAK_RED))
    {
        //pretty much at maximum, no more we can do
        return FALSE;
    }

    //now check for yellow alert status
    if ((!bitTest(aiCurrentAIPlayer->AlertStatus, ALERT_CLOAK_YELLOW)) &&
        // CAT: the two cloak generators, no cloaked fighter (aiuFindCloakersInEnemyShipsIAmAwareOf 0x0041f770: 0x3f, 0x4d)
        ((aiCurrentAIPlayer->enemyShipsIAmAwareOf[kCloakGenerator].selection->numShips) ||
         (aiCurrentAIPlayer->enemyShipsIAmAwareOf[tCloakGenerator].selection->numShips) ||
         (aiCurrentAIPlayer->primaryEnemyShipsIAmAwareOf[kCloakGenerator].selection->numShips) ||
         (aiCurrentAIPlayer->primaryEnemyShipsIAmAwareOf[tCloakGenerator].selection->numShips)))
    {
        bitSet(aiCurrentAIPlayer->AlertStatus, ALERT_CLOAK_YELLOW);
        change = TRUE;
    }

    if (Red_Alert && aiuFindDecloakedShipInSphereOfInfluence())
    {
        bitSet(aiCurrentAIPlayer->AlertStatus, ALERT_CLOAK_RED);
        bitSet(aiCurrentAIPlayer->AlertStatus, ALERT_CLOAK_YELLOW);
        change = TRUE;
    }
    return change;
}




/*=============================================================================
    Blob Utility Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : aiuGetNumEnemyBlobs
    Description : Returns the number of enemy blobs in the universe
    Inputs      :
    Outputs     :
    Return      : The number of enemy blobs recorded by the computer player
----------------------------------------------------------------------------*/
udword aiuGetNumEnemyBlobs(void)
{
    return aiuEnemyBlobs->numBlobs;
}



/*-----------------------------------------------------------------------------
    Name        : aiuUpdateKnowledgeOfEnemyShips
    Description : Updates a list of ships seen to date by the computer player
    Inputs      : aiplayer - the current aiplayer
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aiuUpdateKnowledgeOfEnemyShips(AIPlayer *aiplayer)
{
    Ship *ship;
    udword i, j;
    SelectCommand *blobShips;

    for (i=0;i<aiuEnemyBlobs->numBlobs;i++)
    {
        if (aiuEnemyBlobs->blob[i]->visibility)
        {
            blobShips = aiuEnemyBlobs->blob[i]->blobShips;

            for (j=0;j<blobShips->numShips;j++)
            {
                ship = blobShips->ShipPtr[j];
                // CAT: the crystals and slipgates are not remembered (aiuUpdateKnowledgeOfEnemyShips 0x0041f810: 0x7e 0x7f
                //      0x70 0x71 0x72)
                if ((ship->shiptype == dCrystal0) || (ship->shiptype == dCrystal1) || (ship->shiptype == dSlipgateSmall) ||
                    (ship->shiptype == dSlipgateMedium) || (ship->shiptype == dSlipgateLarge))
                {
                    continue;
                }
                if (ship->playerowner == aiplayer->primaryEnemyPlayer)
                {
                    growSelectAddShipNoDuplication(&aiplayer->primaryEnemyShipsIAmAwareOf[ship->shiptype], ship);
                }
                else if (!allianceArePlayersAllied(ship->playerowner,aiplayer->player))
                {
                    growSelectAddShipNoDuplication(&aiplayer->enemyShipsIAmAwareOf[ship->shiptype], ship);
                }
            }
        }
    }
}




/*-----------------------------------------------------------------------------
    Name        : aiuFillInArrays
    Description : Fills in the blob arrays with blobs containing the player's ships
                  and blobs containing the enemy players' ships (later resources)
    Inputs      : goodGuy_blob_array - an array of blobs containing the player's ships
                  enemy_blob_array - an array of blobs containing the enemy players' ships
    Outputs     : Fills in the blob arrays
    Return      : void
----------------------------------------------------------------------------*/
void aiuFillInArrays(blob_array *goodGuy_blob_array, blob_array *enemy_blob_array, Player *player)
{
    blob *tempBlob;
    SelectCommand *tempShips;
    Node *blobnode = universe.collBlobList.head;
    ShipPtr ship;
    udword i;
    bool32 enemyFound = FALSE, goodGuyFound = FALSE, myMothershipFound = FALSE;

    //go through the list of blobs
    while (blobnode != NULL)
    {
        tempBlob  = (blob *)listGetStructOfNode(blobnode);
        tempShips = tempBlob->blobShips;

        //check each ship in the blob list
        for (i = 0; i < tempShips->numShips; i++)
        {
            ship = tempShips->ShipPtr[i];

            //as soon as an enemy ship is found, stick the blob into the blob array, don't do this more than once per blob
            // CAT: an enemy ship counts when it is not fully cloaked, the computer player has detected it, it is
            //      targetable and not a crystal (aiuFillInArrays 0x0041f900: cat_shipIsCloakedFully, sub_53F9F0 =
            //      cat_shipDetectedByPlayer, cat_shipIsUntargetable; types 0x7e, 0x7f); HW1 asked the proximity sensors
            if ((!enemyFound) && (ship->playerowner != player) &&
                (!cat_shipIsCloakedFully(ship)) &&
                cat_shipDetectedByPlayer(ship, aiCurrentAIPlayer->player->playerIndex) &&
                (!cat_shipIsUntargetable(ship)) &&
                (ship->shiptype != dCrystal0) && (ship->shiptype != dCrystal1))
            {
                enemy_blob_array->blob[enemy_blob_array->numBlobs] = tempBlob;
                enemy_blob_array->numBlobs++;
                enemyFound = TRUE;
            }
            //as soon as a goodguy ship is found, stick the blob into the blob array, don't do this more than once per blob
            else if ((!goodGuyFound) && (ship->playerowner == player))
            {
                goodGuy_blob_array->blob[goodGuy_blob_array->numBlobs] = tempBlob;
                goodGuy_blob_array->numBlobs++;
                goodGuyFound = TRUE;
            }
            if ((!myMothershipFound) && (ship == player->PlayerMothership) &&
                (ship->playerowner == player))
            {
                if (!myMothershipBlob)
                {
                    myMothershipBlob = (aiblob *)memAlloc(sizeof(aiblob), "moshipblob", 0);
                }
                blob_to_aiblob(tempBlob, myMothershipBlob);
                myMothershipFound = TRUE;
            }
        }
        blobnode     = blobnode->next;
        goodGuyFound = FALSE;
        enemyFound   = FALSE;
    }

    if (!myMothershipFound)
    {
        aiumemFree(myMothershipBlob);
    }
}



/*-----------------------------------------------------------------------------
    Name        : aiuFillInAIBlobArrays
    Description : Fills in the aiblob arrays
    Inputs      : enemy_blob_array - blob array of blobs with enemy ships present
                  goodGuy_blob_array - blob array of blobs with good guy ships present
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aiuFillInAIBlobArrays(blob_array *goodGuy_blob_array, blob_array *enemy_blob_array, Player *player)
{
    SelectCommand *tempShips;
    udword i, j;

    //allocate the global blob arrays
    aiuEnemyBlobs   = (aiblob_array *)memAlloc(sizeof_aiblob_array(enemy_blob_array->numBlobs), "enbs", Pyrophoric);
    aiuGoodGuyBlobs = (aiblob_array *)memAlloc(sizeof_aiblob_array(goodGuy_blob_array->numBlobs), "ggbs", Pyrophoric);

    //transfer over the results from above over to the global blob array
    for (i = 0, j = 0; i < enemy_blob_array->numBlobs; i++)
    {
        //first eliminate blobs that have no non-cloaked enemy ships in them
        tempShips = selectMemDupSelection(enemy_blob_array->blob[i]->blobShips, "fibe", Pyrophoric);
        MakeSelectionNotHaveNonVisibleEnemyShips(tempShips, player);

        //if enemy blob still has any visible ships, store in EnemyBlobs array, else don't
        if (tempShips->numShips)
        {
            aiuEnemyBlobs->blob[j] = (aiblob *)memAlloc(sizeof(aiblob), "aieb", Pyrophoric);
            blob_to_aiblob(enemy_blob_array->blob[i], aiuEnemyBlobs->blob[j]);
            aiuEnemyBlobs->blob[j]->blobShips = tempShips;
            j++;
        }
        else
        {
            memFree(tempShips);     // CAT: the exe frees an emptied selection (aiuFillInAIBlobArrays 0x0041fc3f)
        }
    }
    aiuEnemyBlobs->numBlobs = j;

    for (i = 0; i < goodGuy_blob_array->numBlobs; i++)
    {
        tempShips = selectMemDupSelection(goodGuy_blob_array->blob[i]->blobShips, "fibg", Pyrophoric);
        MakeSelectionNotHaveNonVisibleEnemyShips(tempShips, player);

        //no need to check if tempships has ships left over (as with enemy blobs)
        //because good guy blobs will always have at least one good guy (and
        //therefore visible) ship
        aiuGoodGuyBlobs->blob[i] = (aiblob *)memAlloc(sizeof(aiblob), "aigb", Pyrophoric);
        blob_to_aiblob(goodGuy_blob_array->blob[i], aiuGoodGuyBlobs->blob[i]);
        aiuGoodGuyBlobs->blob[i]->blobShips = tempShips;

    }
    aiuGoodGuyBlobs->numBlobs = i;

}

#define AIU_PLAYERMOTHERSHIP_FACTOR     1.5

/*-----------------------------------------------------------------------------
    Name        : aiuRateBlobs
    Description : Rates each blob for enemy ship strength and all that jazz
    Inputs      : player - the current computer player's player structure
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aiuRateBlobs(Player *player)
{
    udword i,j;
    udword primaryenemystrength, primaryenemyvalue;
    udword otherenemystrength, otherenemyvalue;
    udword goodGuystrength, goodGuyvalue;
    udword tempstrength, tempvalue;
    udword goodGuyResourcers;
    bool32   mothership;
    SelectCommand *ships;
    ShipPtr ship;
    Player *shipplayer;
    Player *primaryenemy = player->aiPlayer->primaryEnemyPlayer;
    aiblob *tempblob;

    //for every blob with good guy ships in it
    for (i=0,primaryenemystrength = 0,
             primaryenemyvalue    = 0,
             otherenemystrength   = 0,
             otherenemyvalue      = 0,
             goodGuystrength      = 0,
             goodGuyvalue         = 0,
             goodGuyResourcers    = 0,
             mothership           = FALSE;
         i<aiuGoodGuyBlobs->numBlobs;
         i++,primaryenemystrength = 0,
             primaryenemyvalue    = 0,
             otherenemystrength   = 0,
             otherenemyvalue      = 0,
             goodGuystrength      = 0,
             goodGuyvalue         = 0,
             goodGuyResourcers    = 0,
             mothership           = FALSE)
    {
        tempblob   = aiuGoodGuyBlobs->blob[i];
        ships      = tempblob->blobShips;

        for (j=0,tempstrength=0,tempvalue=0;j<ships->numShips;j++)
        {
            ship       = ships->ShipPtr[j];
            shipplayer = ship->playerowner;

            aiuRateShip(&tempstrength,&tempvalue, ship);

            if ((shipplayer->PlayerMothership) &&
                (ship == shipplayer->PlayerMothership))
            {
                // CAT: rounded to the nearest (the double-magic sequence of aiuRateBlobs 0x0041fd90), HW1 truncated a float
                tempvalue  = cat_aiuRoundToUdword((double)tempvalue * AIU_PLAYERMOTHERSHIP_FACTOR);
                mothership = TRUE;
            }

            if (shipplayer == player)
            {
                goodGuystrength += tempstrength;
                goodGuyvalue    += tempvalue;

                if (ship->staticinfo->shipclass == CLASS_Resource)     // CAT: the class (aiuRateBlobs 0x0041fd90), HW1's ResourceCollector
                {
                    goodGuyResourcers++;
                }
            }
            else if (shipplayer == primaryenemy)
            {
                primaryenemystrength += tempstrength;
                primaryenemyvalue    += tempvalue;
            }
            else
            {
                otherenemystrength += tempstrength;
                otherenemyvalue    += tempvalue;
            }
        }
        tempblob->primaryenemystrength = primaryenemystrength;
        tempblob->primaryenemyvalue    = primaryenemyvalue;
        tempblob->otherenemystrength   = otherenemystrength;
        tempblob->otherenemyvalue      = otherenemyvalue;
        tempblob->goodGuystrength      = goodGuystrength;
        tempblob->goodGuyvalue         = goodGuyvalue;
        tempblob->goodGuyResourcers    = goodGuyResourcers;
        tempblob->visibility           = TRUE;
        tempblob->mothership           = mothership;

//        dbgAssertOrIgnore(tempblob->goodGuyvalue);
    }

    for (i=0,goodGuystrength      = 0,
             goodGuyvalue         = 0,
             primaryenemystrength = 0,
             primaryenemyvalue    = 0,
             otherenemystrength   = 0,
             otherenemyvalue      = 0,
             goodGuyResourcers    = 0,
             mothership           = FALSE;
         i<aiuEnemyBlobs->numBlobs;
         i++,primaryenemystrength = 0,
             primaryenemyvalue    = 0,
             otherenemystrength   = 0,
             otherenemyvalue      = 0,
             goodGuystrength      = 0,
             goodGuyvalue         = 0,
             goodGuyResourcers    = 0,
             mothership           = FALSE)
    {
        tempblob = aiuEnemyBlobs->blob[i];
        ships    = tempblob->blobShips;

        for (j=0;j<ships->numShips;j++)
        {
            ship       = ships->ShipPtr[j];
            shipplayer = ship->playerowner;

            aiuRateShip(&tempstrength, &tempvalue, ship);

            if ((shipplayer->PlayerMothership) &&
                (ship == shipplayer->PlayerMothership))
            {
                // CAT: rounded to the nearest (the double-magic sequence of aiuRateBlobs 0x0041fd90), HW1 truncated a float
                tempvalue  = cat_aiuRoundToUdword((double)tempvalue * AIU_PLAYERMOTHERSHIP_FACTOR);
                mothership = TRUE;
            }

            // CAT: no HW1 CryoTray (M2-R4 ships-b)


            if (shipplayer == player)
            {
                goodGuystrength += tempstrength;
                goodGuyvalue    += tempvalue;
                tempblob->visibility = TRUE;

                if (ship->staticinfo->shipclass == CLASS_Resource)     // CAT: the class (aiuRateBlobs 0x0041fd90), HW1's ResourceCollector
                {
                    goodGuyResourcers++;
                }
            }
            else if (shipplayer == primaryenemy)
            {
                primaryenemystrength += tempstrength;
                primaryenemyvalue    += tempvalue;
            }
            else
            {
                otherenemystrength += tempstrength;
                otherenemyvalue    += tempvalue;
            }
        }
        tempblob->primaryenemystrength = primaryenemystrength;
        tempblob->primaryenemyvalue    = primaryenemyvalue;
        tempblob->otherenemystrength   = otherenemystrength;
        tempblob->otherenemyvalue      = otherenemyvalue;
        tempblob->goodGuystrength      = goodGuystrength;
        tempblob->goodGuyvalue         = goodGuyvalue;
        tempblob->goodGuyResourcers    = goodGuyResourcers;
        tempblob->mothership           = mothership;

        dbgAssertOrIgnore((tempblob->primaryenemyvalue) || (tempblob->otherenemyvalue));
    }
}



/*-----------------------------------------------------------------------------
    Name        : aiuCreateBlobArrays
    Description : Creates two array of pointers to all blobs containing enemy
                  and good guy ships
    Inputs      :
    Outputs     : Builds the aiuEnemyBlobs array and the aiuGoodGuyBlobs array
    Return      : void
----------------------------------------------------------------------------*/
void aiuCreateBlobArrays(Player *player)
{
    blob_array *enemy_blob_array, *goodGuy_blob_array;

    enemy_blob_array   = (blob_array *)memAlloc(sizeof_blob_array(universe.collBlobList.num), "tpb1", Pyrophoric);
    goodGuy_blob_array = (blob_array *)memAlloc(sizeof_blob_array(universe.collBlobList.num), "tpb2", Pyrophoric);

    enemy_blob_array->numBlobs   = 0;
    goodGuy_blob_array->numBlobs = 0;

    aiuFillInArrays(goodGuy_blob_array, enemy_blob_array, player);
    aiuFillInAIBlobArrays(goodGuy_blob_array, enemy_blob_array, player);
    aiuRateBlobs(player);

    memFree(enemy_blob_array);
    memFree(goodGuy_blob_array);
}


/*-----------------------------------------------------------------------------
    Name        : aiuDeleteBlobArrays
    Description : Deletes the two blob arrays
    Inputs      :
    Outputs     : Deallocates the memory taken up by the blob arrays
    Return      : void
----------------------------------------------------------------------------*/
void aiuDeleteBlobArrays(void)
{
    udword i;

    for (i=0;i<aiuEnemyBlobs->numBlobs;i++)
    {
        memFree(aiuEnemyBlobs->blob[i]->blobShips);
        memFree(aiuEnemyBlobs->blob[i]);
    }
    memFree(aiuEnemyBlobs);

    for (i=0;i<aiuGoodGuyBlobs->numBlobs;i++)
    {
        memFree(aiuGoodGuyBlobs->blob[i]->blobShips);
        memFree(aiuGoodGuyBlobs->blob[i]);
    }
    memFree(aiuGoodGuyBlobs);

    aiumemFree(myMothershipBlob);
}




