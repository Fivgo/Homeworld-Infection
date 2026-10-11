/*=============================================================================
    Name    : ShipSelect.c
    Purpose : Routines for manipulating SelectCommand structures

    Created 10/19/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "ShipSelect.h"

#include <string.h>

#include "Alliance.h"
#include "Attributes.h"
#include "Battle.h"
#include "CommandDefs.h"
#include "Cloak.h"
#include "CommandLayer.h"
#include "InfoOverlay.h"
#include "Memory.h"
#include "MultiplayerGame.h"
#include "MaxMultiplayer.h"
#include "Select.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "SpeechEvent.h"
#include "Tweak.h"
#include "Universe.h"

// CAT: Cataclysm formation types 9 and 10 (the exe tests command->formation.formationtype == 9 / == 10, e.g.
//      isShipPassiveAttackCapable 0x00511b10, cat_shipIsSentinelInField 0x005c6ec0); their names and the formations
//      themselves come with the Formation port (M2-R4 #2), so no command carries them yet
#define CAT_FORMATION_9     9
#define CAT_FORMATION_10    10

// CAT: Cataclysm-only command orders after COMMAND_MP_HYPERSPACING (10) that cat_selMakeShipsProtectCapable 0x00511f00
//      tests; their names come with the CommandLayer port (M2-R4 #2)
#define CAT_COMMAND_11      11
#define CAT_COMMAND_12      12
#define CAT_COMMAND_14      14
#define CAT_COMMAND_15      15
#define CAT_COMMAND_16      16
#define CAT_COMMAND_17      17

/*-----------------------------------------------------------------------------
    Name        : AddSpaceObjToSelectionBeforeIndex
    Description : Adds a SpaceObj to selection before index
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void AddSpaceObjToSelectionBeforeIndex(SpaceObj *obj,SpaceObjSelection *selection,sdword index)
{
    sdword i;

    dbgAssertOrIgnore(index >= 0);
    dbgAssertOrIgnore(index < selection->numSpaceObjs);

    for (i=selection->numSpaceObjs;i>index;i--)
    {
        selection->SpaceObjPtr[i] = selection->SpaceObjPtr[i-1];
    }
    selection->SpaceObjPtr[index] = obj;
    selection->numSpaceObjs++;
}

/*-----------------------------------------------------------------------------
    Name        : AddSpaceObjToSelectionAfterIndex
    Description : Adds a SpaceObj to selection after index
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void AddSpaceObjToSelectionAfterIndex(SpaceObj *obj,SpaceObjSelection *selection,sdword index)
{
    sdword i;

    dbgAssertOrIgnore(index >= 0);
    dbgAssertOrIgnore(index < selection->numSpaceObjs);

    index++;

    for (i=selection->numSpaceObjs;i>index;i--)
    {
        selection->SpaceObjPtr[i] = selection->SpaceObjPtr[i-1];
    }
    selection->SpaceObjPtr[index] = obj;
    selection->numSpaceObjs++;
}

/*-----------------------------------------------------------------------------
    Name        : RemoveSpaceObjFromSelectionPreserveOrder
    Description : removes a space obj from the selection, while preserving order
    Inputs      : selection, obj
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 RemoveSpaceObjFromSelectionPreserveOrder(SpaceObjSelection *selection,SpaceObj *obj)
{
    sdword i;
    sdword numSpaceObjs = selection->numSpaceObjs;

    for (i=0;i<numSpaceObjs;i++)
    {
        if (obj == selection->SpaceObjPtr[i])
        {
            for ( ;i<numSpaceObjs-1;i++)
            {                                           //for the rest of the list
                selection->SpaceObjPtr[i] = selection->SpaceObjPtr[i+1];
            }
            selection->numSpaceObjs--;

            return TRUE;
        }
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : RemoveTargetFromSelection
    Description : A useful utility to remove targets from selection
                  Does not delete entry - just removes reference to it
    Inputs      : removeTargetPtr
    Outputs     :
    Return      : Returns TRUE if any target was removed
----------------------------------------------------------------------------*/
bool32 clRemoveTargetFromSelection(SelectAnyCommand *selection,TargetPtr removeTargetPtr)
{
    sdword i;
    bool32 removedAny = FALSE;

    if (selection == NULL) {
        return FALSE;
    }

    for (i=0;i<selection->numTargets; )
    {
        if (removeTargetPtr == selection->TargetPtr[i])
        {
            removedAny = TRUE;
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numTargets--;
            selection->TargetPtr[i] = selection->TargetPtr[selection->numTargets];

            continue; // we must check same index again because we put last array entry here
                      // warning: leave this as continue because sometimes removeTargetPtr may be set to NULL to remove all null's
        }
        i++;
    }

    return removedAny;
}

/*-----------------------------------------------------------------------------
    Name        : ShipInSelection
    Description : returns TRUE if ship is in selection
    Inputs      : selection, ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 ShipInSelection(SelectCommand *selection,Ship *ship)
{
    sdword i;

    for (i=0;i<selection->numShips;i++)
    {
        if (selection->ShipPtr[i] == ship)
        {
            return TRUE;
        }
    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : ShiptypeInSelection
    Description : Returns TRUE if the selection contains a ship of shiptype "Type"
    Inputs      : selection - the selection to check, type - the type to check for
    Outputs     :
    Return      : TRUE if a ship of the specified type is found
----------------------------------------------------------------------------*/
udword ShiptypeInSelection(SelectCommand *selection, ShipType type)
{
    udword i, count;

    for (i=0,count=0;i<selection->numShips;i++)
    {
        if (selection->ShipPtr[i]->shiptype == type)
        {
            count++;
        }
    }
    return count;
}


/*-----------------------------------------------------------------------------
    Name        : FindFirstInstanceOfShipType
    Description : Returns the first ship in the selection of the specified type
    Inputs      : selection - the selection to check, type - the type to check for
    Outputs     :
    Return      : A pointer to the first instance of the specified type
----------------------------------------------------------------------------*/
ShipPtr FindFirstInstanceOfShipType(SelectCommand *selection, ShipType type)
{
    udword i;

    for (i = 0; i<selection->numShips; i++)
    {
        if (selection->ShipPtr[i]->shiptype == type)
        {
            return selection->ShipPtr[i];
        }
    }
    return NULL;
}




/*-----------------------------------------------------------------------------
    Name        : MakeShipsNotIncludeTheseShips
    Description : Makes sure that ships in selection do not include theseships
    Inputs      : selection, theseships
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeShipsNotIncludeTheseShips(SelectCommand *selection,SelectCommand *theseships)
{
    sdword numTheseShips = theseships->numShips;
    sdword i;

    for (i=0;i<numTheseShips;i++)
    {
        clRemoveShipFromSelection(selection,theseships->ShipPtr[i]);
    }

    ioUpdateShipTotals();   // CAT: every exe filter ends with ioUpdateShipTotals (MakeShipsNotIncludeTheseShips 0x00511940)
}

/*-----------------------------------------------------------------------------
    Name        : AnyOfTheseShipsAreInSelection
    Description : returns true if any ships in theseShips are in selection
    Inputs      : theseShips,selection
    Outputs     :
    Return      : returns true if any ships in theseShips are in selection
----------------------------------------------------------------------------*/
bool32 AnyOfTheseShipsAreInSelection(SelectCommand *theseShips,SelectCommand *selection)
{
    sdword i,j;
    ShipPtr thisShip;

    for (i=0;i<theseShips->numShips;i++)
    {
        thisShip = theseShips->ShipPtr[i];

        for (j=0;j<selection->numShips;j++)
        {
            if (thisShip == selection->ShipPtr[j])
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : TheseShipsAreInSelection
    Description : returns true if all of these ships are in selection
    Inputs      : theseShips, selection
    Outputs     :
    Return      : returns true if all of these ships are in selection
----------------------------------------------------------------------------*/
bool32 TheseShipsAreInSelection(SelectCommand *theseShips,SelectCommand *selection)
{
    sdword i;
    sdword numShips = theseShips->numShips;

    if (numShips == 0)
    {
        return FALSE;
    }

    for (i=0;i<numShips;i++)
    {
        if (!ShipInSelection(selection,theseShips->ShipPtr[i]))
        {
            return FALSE;
        }
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : SelectionsAreEquivalent
    Description : tells whether selection1 and selection2 are focusing on the same
                  ships (ignores order of objects in lists).
    Inputs      : selection1, selection2
    Outputs     :
    Return      : true if selection1 and selection2 are focusing on the same ships
----------------------------------------------------------------------------*/
bool32 SelectionsAreEquivalent(SelectCommand *selection1,SelectCommand *selection2)
{
    sdword i,j;
    sdword numShips = selection1->numShips;
    ShipPtr matchship;
    bool32 matched;

    if (numShips != selection2->numShips)
    {
        return FALSE;
    }

    for (i=0;i<numShips;i++)
    {
        matched = FALSE;
        matchship = selection1->ShipPtr[i];

        for (j=0;j<numShips;j++)
        {
            if (selection2->ShipPtr[j] == matchship)
            {
                matched = TRUE;
                break;
            }
        }

        if (!matched)
        {
            return FALSE;
        }
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : SelectionsAreTotallyEquivalent
    Description : tells whether selection1 and selection2 are the same selections
                  (including order).
    Inputs      : selection1, selection2
    Outputs     :
    Return      : true if selection1 and selection2 are the same
----------------------------------------------------------------------------*/
bool32 SelectionsAreTotallyEquivalent(SelectCommand *selection1,SelectCommand *selection2)
{
    sdword numShips = selection1->numShips;
    sdword i;

    if (numShips != selection2->numShips)
    {
        return FALSE;
    }

    for (i=0;i<numShips;i++)
    {
        if (selection1->ShipPtr[i] != selection2->ShipPtr[i])
        {
            return FALSE;
        }
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : isShipAttackCapable
    Description : returns TRUE if ship is attack capable
    Inputs      : ship
    Outputs     :
    Return      : returns TRUE if ship is attack capable
----------------------------------------------------------------------------*/
bool32 isShipAttackCapable(Ship *ship)
{
    if (ship->staticinfo->custshipheader.CustShipAttack == NULL)
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : AreAllShipsAttackCapable
    Description : returns TRUE if all ships are attack capable
    Inputs      : selection
    Outputs     :
    Return      : returns TRUE if all ships are attack capable
----------------------------------------------------------------------------*/
bool32 AreAllShipsAttackCapable(SelectCommand *selection)
{
    sdword i;
    ShipStaticInfo *shipstatic;

    for (i=0;i<selection->numShips;i++)
    {
        shipstatic = selection->ShipPtr[i]->staticinfo;
        if (shipstatic->custshipheader.CustShipAttack == NULL)
        {
            return FALSE;
        }
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : AreAnyShipsAttackCapable
    Description : returns TRUE if any ships are attack capable
    Inputs      : selection
    Outputs     :
    Return      : returns TRUE if any ships are attack capable
----------------------------------------------------------------------------*/
bool32 AreAnyShipsAttackCapable(SelectCommand *selection)
{
    sdword i;
    ShipStaticInfo *shipstatic;

    for (i=0;i<selection->numShips;i++)
    {
        shipstatic = selection->ShipPtr[i]->staticinfo;
        if (shipstatic->custshipheader.CustShipAttack != NULL)
        {
            return TRUE;
        }
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : isShipPassiveAttackCapable
    Description : returns TRUE if ship is attack capable
    Inputs      : ship
    Outputs     :
    Return      : returns TRUE if ship is attack capable
----------------------------------------------------------------------------*/
bool32 isShipPassiveAttackCapable(Ship *ship)
{
    // CAT: a ship in one of the two new formations (sentinel fields) or being built cannot passive-attack
    //      (isShipPassiveAttackCapable 0x00511b10)
    if ((ship->command != NULL) && (ship->command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
        ((ship->command->formation.formationtype == CAT_FORMATION_10) ||
         (ship->command->formation.formationtype == CAT_FORMATION_9)))
    {
        return FALSE;
    }
    if (bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
    {
        return FALSE;
    }

    if (ship->staticinfo->custshipheader.CustShipAttackPassive == NULL)
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : AreAllShipsPassiveAttackCapable
    Description : returns TRUE if all ships are attack capable
    Inputs      : selection
    Outputs     :
    Return      : returns TRUE if all ships are attack capable
----------------------------------------------------------------------------*/
bool32 AreAllShipsPassiveAttackCapable(SelectCommand *selection)
{
    sdword i;
    ShipStaticInfo *shipstatic;

    for (i=0;i<selection->numShips;i++)
    {
        shipstatic = selection->ShipPtr[i]->staticinfo;
        if (shipstatic->custshipheader.CustShipAttackPassive == NULL)
        {
            return FALSE;
        }
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : AreAnyShipsPassiveAttackCapable
    Description : returns TRUE if any ships are attack capable
    Inputs      : selection
    Outputs     :
    Return      : returns TRUE if any ships are attack capable
----------------------------------------------------------------------------*/
bool32 AreAnyShipsPassiveAttackCapable(SelectCommand *selection)
{
    sdword i;
    ShipStaticInfo *shipstatic;

    for (i=0;i<selection->numShips;i++)
    {
        shipstatic = selection->ShipPtr[i]->staticinfo;
        if (shipstatic->custshipheader.CustShipAttackPassive != NULL)
        {
            return TRUE;
        }
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : MakeShipsAttackCapable
    Description : Makes sure that the ships in this selection are capable of
                  attack.  Removes any ships from this selection that are not
                  capable of attacking.
    Inputs      : source - selection to cull
                  skipDisabled - CAT: also drop disabled ships (the UI callers pass TRUE, the game code FALSE)
    Outputs     : dest - culled selection
    Return      : number of ships in culled selection
    Note        : source and dest can safely be the same selection.
----------------------------------------------------------------------------*/
sdword MakeShipsAttackCapable(SelectCommand *dest, SelectCommand *source, bool32 skipDisabled)
{
    sdword index, numShips = 0;
    Ship *ship;

    for (index = 0; index < source->numShips; index++)
    {
        ship = source->ShipPtr[index];
        // CAT: a Sentinel holding a field and (with skipDisabled) a disabled ship are not attack capable
        //      (MakeShipsAttackCapable 0x00511b60)
        if ((ship->staticinfo->custshipheader.CustShipAttack != NULL) &&
            (!cat_shipIsSentinelInField(ship)) &&
            ((!skipDisabled) || (!ship->catDisabled)))
        {
            dest->ShipPtr[numShips] = ship;
            numShips++;
        }
    }
    dest->numShips = numShips;
    ioUpdateShipTotals();       // CAT: MakeShipsAttackCapable 0x00511b60
    return(numShips);
}

/*-----------------------------------------------------------------------------
    Name        : ShipCanHarvest
    Description : returns TRUE if ship can harvest
    Inputs      :
    Outputs     :
    Return      : returns TRUE if ship can harvest
----------------------------------------------------------------------------*/
bool32 ShipCanHarvest(ShipStaticInfo *shipstatic)
{
    // CAT: any resource-class ship with a harvest amount harvests (ShipCanHarvest 0x00513680); HW1 tested
    //      for its resource-collector type
    if ((shipstatic->shipclass == CLASS_Resource) && (shipstatic->harvestAmount > 0))
    {
        return TRUE;
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : MakeShipsHarvestCapable
    Description : Makes a selection of just ships that are harvesters.
    Inputs      : source - selection to search through
                  workersOnly - CAT: also require a non-disabled Worker (the UI caller mrHarvestResources passes TRUE)
    Outputs     : dest - where to store the culled selection
    Return      : number of ships in culled selection
----------------------------------------------------------------------------*/
sdword MakeShipsHarvestCapable(SelectCommand *dest, SelectCommand *source, bool32 workersOnly)
{
    sdword index, numShips = 0;
    Ship *ship;

    for (index = 0; index < source->numShips; index++)
    {
        ship = source->ShipPtr[index];
        // CAT: MakeShipsHarvestCapable 0x00511be0: ShipCanHarvest inlined, harvesting must be on (single player or
        //      MG_HarvestinEnabled), and with workersOnly the ship must be a non-disabled sWorker/bWorker
        if (ShipCanHarvest(ship->staticinfo) &&
            (singlePlayerGame || bitTest(tpGameCreated.flag, MG_HarvestinEnabled)) &&
            ((!workersOnly) ||
             ((!ship->catDisabled) && ((ship->shiptype == sWorker) || (ship->shiptype == bWorker)))))
        {
            dest->ShipPtr[numShips] = ship;
            numShips++;
        }
    }
    dest->numShips = numShips;
    ioUpdateShipTotals();       // CAT: MakeShipsHarvestCapable 0x00511be0
    return(numShips);
}

/*-----------------------------------------------------------------------------
    Name        : ShipCanGuard
    Description : returns TRUE if ship is capable of guarding
    Inputs      :
    Outputs     :
    Return      : returns TRUE if ship is capable of guarding
----------------------------------------------------------------------------*/
bool32 ShipCanGuard(ShipStaticInfo *shipstatic)
{
    // CAT: MakeShipsGuardCapable 0x00511c70 inlines this test: no mothership-class or component-class ship and no
    //      externally built ship guards; HW1 tested its mothership and probe types
    if ((shipstatic->shipclass == CLASS_Mothership) || (shipstatic->shipclass == CLASS_Component))
    {
        return FALSE;
    }

    if (shipstatic->externalBuild)
    {
        return FALSE;
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : MakeShipsGuardCapable
    Description : Makes sure that the ships in this selection are capable of
                  guarding.  Removes any ships from this selection that are not
                  capable of guarding.
    Inputs      : selection
                  skipDisabled - CAT: also drop disabled ships (the UI callers pass TRUE)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeShipsGuardCapable(SelectCommand *selection, bool32 skipDisabled)
{
    sdword i;
    ShipStaticInfo *shipstatic;

    for (i=0;i<selection->numShips;)
    {
        shipstatic = (ShipStaticInfo *)selection->ShipPtr[i]->staticinfo;
        if ((!ShipCanGuard(shipstatic)) ||
            (skipDisabled && selection->ShipPtr[i]->catDisabled))      // CAT: MakeShipsGuardCapable 0x00511c70
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];

            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: MakeShipsGuardCapable 0x00511c70
}

/*-----------------------------------------------------------------------------
    Name        : makeShipsDockCapable
    Description : removes ships that shouldn't be docking from the selection
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void makeShipsDockCapable(SelectCommand *selection)
{
    // CAT: HW1 removed its probe, target-drone and sensor-array types here. Cataclysm has none of the three (the exe has no
    //      makeShipsDockCapable; its UI filters dock orders with cat_selMakeShipsDockCapableFor 0x00513710), so this
    //      removes nothing. Kept for its unported HW1 callers (mainrgn.c, mouse.c: stage 3).
    (void)selection;
}

/*-----------------------------------------------------------------------------
    Name        : makeShipsRetireable
    Description : removes ships that shouldn't be retiring from the selection
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void makeShipsRetireable(SelectCommand *selection)
{
    sdword i;
    Ship *ship;

    for (i=0;i<selection->numShips;)
    {
        ship = selection->ShipPtr[i];
        // CAT: the exe removes the Taiidan resource collector (makeShipsRetireable 0x00511db0, type 0x58); HW1 removed
        //      its probe and target-drone types
        if (ship->shiptype == tResourceCollector)
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];

            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: makeShipsRetireable 0x00511db0
}

/*-----------------------------------------------------------------------------
    Name        : makeShipsNotIncludeSinglePlayerMotherships
    Description : If in single player, removes motherships from the selection
    Inputs      : selection - selection to cull
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void makeShipsNotIncludeSinglePlayerMotherships(SelectCommand *selection)
{
    // CAT: the exe has no such function; HW1's body (MakeShipsMobile) is kept, without the new disabled-ship test
    MakeShipsMobile(selection, FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : makeShipsControllable
    Description : Removes ships from the selection such that all ships left in the selction
                  aren't docking and can be freely ordered
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void makeShipsControllable(SelectCommand *selection,sdword newCommand)
{
    sdword i;
    CommandToDo *command;

    if (playPackets)
    {
        // playing back game, user should be able to do NOTHING
        //selection->numShips = 0;
        return;
    }

    if(newCommand == COMMAND_HALT)
    {
        return;
    }
    for (i=0;i<selection->numShips;)
    {
        command = getShipAndItsCommand(&universe.mainCommandLayer,selection->ShipPtr[i]);
        if(command != NULL)
        {
            if(command->ordertype.order == COMMAND_DOCK ||
                command->ordertype.order == COMMAND_LAUNCH_SHIP)
            {
                //if docking ship is NULL, we can assume some stuff...
                if (selection->ShipPtr[i]->shipOrDerelict.dockingship != NULL)
                {
                    // to remove ShipPtr from list, decrement numShips and put last member
                    // of array into slot being deleted.

                    ///////////////////////
                    //Cannot Comply speech event!
                    //Use Battle Chatter to filter
                    //if SHOULDn't be called for move's that are cancelled..
                    //then we'll add a flag that gets passed it turning this on or off
                    //event num: COMM_CannotComply
                    //BCE_CannotComply
                    ///////////////////////
                    if (battleCanChatterAtThisTime(BCE_CannotComply, selection->ShipPtr[i]->shipOrDerelict.dockingship))
                    {
                        // CAT: the exe speaks for the selected ship, with its type as the variable
                        //      (makeShipsControllable 0x00512020); HW1 spoke for the docking ship
                        battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_CannotComply, selection->ShipPtr[i], selection->ShipPtr[i]->shiptype);
                    }

                    // CAT: no research-ship exception (makeShipsControllable 0x00512020 removes every such ship)
                    selection->numShips--;
                    selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
                    continue; // we must check same index again because we put last array entry here
                }
            }
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: makeShipsControllable 0x00512020
}

/*-----------------------------------------------------------------------------
    Name        : makeSelectionHyperspaceCapable
    Description : removes the ships that may not hyperspace
    Inputs      : selection
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void makeSelectionHyperspaceCapable(SelectCommand *selection)
{
    sdword i, j;
    Ship *ship;
    SelectCommand *clamped;
    bool32 remove;

    //remove all NON-hyperspaceable ships!

    for(i=0;i<selection->numShips;)
    {
        ship = selection->ShipPtr[i];

        // CAT: makeSelectionHyperspaceCapable 0x005120d0. Removed as well: a host with a clamped object still being
        //      built, a ship being salvaged into a depot that holds a crystal, a Sect mothership on a special order or
        //      with its big gun active. Then HW1's canMpHyperspace test.
        remove = FALSE;
        clamped = (ship->guidance.clampInfo != NULL) ? (SelectCommand *)ship->guidance.clampInfo->clampedObjs : NULL;
        if (clamped != NULL)
        {
            for (j = 0; j < clamped->numShips; j++)
            {
                if (bitTest(clamped->ShipPtr[j]->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
                {
                    remove = TRUE;
                }
            }
        }
        if ((!remove) && (ship->header.objtype == OBJ_ShipType) && cat_salvageIsBeingSalvaged(ship) &&
            cat_salvageDepotHoldsCrystal(ship))
        {
            remove = TRUE;
        }
        if (!remove)
        {
            if ((ship->shiptype == sMothership) && (ship->command != NULL) &&
                ((ship->command->ordertype.order == COMMAND_SPECIAL) || cat_mothershipBigGunActive(ship)))
            {
                remove = TRUE;
            }
            else if (!TW_HyperSpaceCostStruct[ship->shiptype].canMpHyperspace)
            {
                remove = TRUE;
            }
        }

        if (remove)
        {
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
            continue;
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: makeSelectionHyperspaceCapable 0x005120d0
}


/*-----------------------------------------------------------------------------
    Name        : MakeShipsSpecialActivateCapable
    Description : Makes sure that the ships in this selection are capable of
                  the special activate.  Removes any ships from this selection that are not
    Inputs      : selection
                  skipDisabled - CAT: also drop disabled ships, except Leeches and Sentinels (the UI callers pass TRUE)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeShipsSpecialActivateCapable(SelectCommand *selection, bool32 skipDisabled)
{
    sdword i;
    Ship *ship;
    ShipStaticInfo *shipstatic;

    for (i=0;i<selection->numShips;)
    {
        ship = selection->ShipPtr[i];
        shipstatic = (ShipStaticInfo *)ship->staticinfo;
        // CAT: MakeShipsSpecialActivateCapable 0x00512250 (types 3, 0x11, 0x34 stay when disabled)
        if ((shipstatic->custshipheader.CustShipSpecialActivate == NULL) ||
            (skipDisabled && ship->catDisabled &&
             (ship->shiptype != sLeech) && (ship->shiptype != sSentinel) && (ship->shiptype != bSentinel)))
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];

            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: MakeShipsSpecialActivateCapable 0x00512250
}

/*-----------------------------------------------------------------------------
    Name        : MakeShipsSpecialTargetCapable
    Description : Makes sure that the ships in this selection are capable of
                  special targeting.  Removes any ships from this selection that are not
    Inputs      : selection - selection to neuter
                  bFriendlies - HW1: if TRUE, only ships that can z-bandbox friendlies. CAT: not used by the exe
                  skipDisabled - CAT: also drop disabled ships (the UI callers pass TRUE)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeShipsSpecialTargetCapable(SelectCommand *selection, bool32 bFriendlies, bool32 skipDisabled)
{
    sdword i;
    ShipStaticInfo *shipstatic;

    // CAT: the exe ignores bFriendlies and canSpecialBandBoxFriendlies: MakeShipsSpecialTargetCapable 0x005122c0 keeps
    //      every ship with a CustShipSpecialTarget (it calls an empty debug print with the string "@@@@@@@@@ Need to
    //      handle canSpecialBa...")
    (void)bFriendlies;

    for (i=0;i<selection->numShips;)
    {
        shipstatic = (ShipStaticInfo *)selection->ShipPtr[i]->staticinfo;
        if ((shipstatic->custshipheader.CustShipSpecialTarget == NULL) ||
            (skipDisabled && selection->ShipPtr[i]->catDisabled))
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];

            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: MakeShipsSpecialTargetCapable 0x005122c0
}

bool32 AreShipsMobile(SelectCommand *selection)
{
    sdword i;
    ShipStaticInfo *shipstatic;

    for (i=0;i<selection->numShips;i++)
    {
        shipstatic = (ShipStaticInfo *)selection->ShipPtr[i]->staticinfo;
        if (shipstatic->common.immobile)
        {
            return FALSE;
        }
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : MakeShipsMobile
    Description : Makes sure that the ships in this selection are mobile
                  Removes any ships from this selection that are not mobile.
    Inputs      : selection
                  skipDisabled - CAT: also drop disabled ships
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeShipsMobile(SelectCommand *selection, bool32 skipDisabled)
{
    sdword i;
    ShipStaticInfo *shipstatic;

    for (i=0;i<selection->numShips;)
    {
        shipstatic = (ShipStaticInfo *)selection->ShipPtr[i]->staticinfo;
        if ((shipstatic->common.immobile) ||
            (skipDisabled && selection->ShipPtr[i]->catDisabled))       // CAT: MakeShipsMobile 0x005126b0
        {
            // CAT: any mothership-class ship of the current player speaks (MakeShipsMobile 0x005126b0 tests shipclass
            //      0); HW1 tested its mothership type
            if ((shipstatic->shipclass == CLASS_Mothership) && (selection->ShipPtr[i]->playerowner == universe.curPlayerPtr))
            {
                speechEventFleet(COMM_F_MoShip_CantMove, 0, universe.curPlayerIndex);
            }

            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];

            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: MakeShipsMobile 0x005126b0
}

/*-----------------------------------------------------------------------------
    Name        : makeShipsFormationCapable
    Description : removes ships that can't go into formation
    Inputs      : selection
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void makeShipsFormationCapable(SelectCommand *selection)
{

    sdword i;
    Ship *ship;
    for (i=0;i<selection->numShips;)
    {
        ship = selection->ShipPtr[i];
        // CAT: disabled ships cannot go into formation (makeShipsFormationCapable 0x00512780); HW1 removed Probes
        if (ship->catDisabled)
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: makeShipsFormationCapable 0x00512780
}

/*-----------------------------------------------------------------------------
    Name        : MakeShipsNonCapital
    Description : Makes sure that the ships in this selection are not capital
                  ships.  Removes any capital ships from this selection.
    Inputs      : selection
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeShipsNonCapital(SelectCommand *selection)
{
    sdword i;

    for (i=0;i<selection->numShips;)
    {
        if (isCapitalShip(selection->ShipPtr[i]))
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];

            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: MakeShipsNonCapital 0x005127c0
}


/*-----------------------------------------------------------------------------
    Name        : MakeShipsOnlyCapital
    Description : Makes sure that the ships in this selection are only capital ships
    Inputs      : selection - the selection to lookat
    Outputs     : Removes non capital ships from the selection
    Return      : void
----------------------------------------------------------------------------*/
void MakeShipsOnlyCapital(SelectCommand *selection)
{
    sdword i;

    for (i=0;i<selection->numShips;)
    {
        if (isCapitalShip(selection->ShipPtr[i]))
        {
            i++;
        }
        else
        {
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
        }
    }
    ioUpdateShipTotals();       // CAT: MakeShipsOnlyCapital 0x00512810
}


/*-----------------------------------------------------------------------------
    Name        : MakeShipsOnlyFollowConstraints
    Description : General routine that makes all ships in selection follow constraints set
                  out by shipConstraintsCB callback function.
    Inputs      : selection, shipConstraintsCB
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeShipsOnlyFollowConstraints(SelectCommand *selection,ShipConstraintsCB shipConstraintsCB)
{
    sdword i;

    for (i=0;i<selection->numShips;)
    {
        if (!shipConstraintsCB(selection->ShipPtr[i]))
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];

            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: MakeShipsOnlyFollowConstraints 0x00512860
}

/*-----------------------------------------------------------------------------
    Name        : DoAllShipsFollowConstraints
    Description : Returns TRUE if all ships in selection follow shipConstraintsCB
    Inputs      : selection, shipConstraintsCB
    Outputs     :
    Return      : Returns TRUE if all ships in selection follow shipConstraintsCB
----------------------------------------------------------------------------*/
bool32 DoAllShipsFollowConstraints(SelectCommand *selection,ShipConstraintsCB shipConstraintsCB)
{
    sdword i;

    for (i=0;i<selection->numShips;i++)
    {
        if (!shipConstraintsCB(selection->ShipPtr[i]))
        {
            return FALSE;
        }
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : DoAnyShipsFollowConstraints
    Description : Returns TRUE if any ships in selection follow shipConstraintsCB
    Inputs      : selection, shipConstraintsCB
    Outputs     :
    Return      : Returns TRUE if any ships in selection follow shipConstraintsCB
----------------------------------------------------------------------------*/
bool32 DoAnyShipsFollowConstraints(SelectCommand *selection,ShipConstraintsCB shipConstraintsCB)
{
    sdword i;

    for (i=0;i<selection->numShips;i++)
    {
        if (shipConstraintsCB(selection->ShipPtr[i]))
        {
            return TRUE;
        }
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : MakeTargetsNotIncludeMissiles
    Description : Makes sure that the targets in selection are not missiles.
    Inputs      : selection
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeTargetsNotIncludeMissiles(SelectAnyCommand *selection)
{
    sdword i;

    for (i=0;i<selection->numTargets;)
    {
        if ((selection->TargetPtr[i])->header.objtype == OBJ_MissileType)
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numTargets--;
            selection->TargetPtr[i] = selection->TargetPtr[selection->numTargets];

            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: MakeTargetsNotIncludeMissiles 0x00512930
}

/*-----------------------------------------------------------------------------
    Name        : MakeTargetsOnlyEnemyShips
    Description : Makes sure that the targets in this selection are ships and are enemies (not curplayer's ships)
    Inputs      : selection,curplayer
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeTargetsOnlyEnemyShips(SelectAnyCommand *selection,Player *curplayer)
{
    sdword i;
    SpaceObjRotImpTarg *target;

    for (i=0;i<selection->numTargets;)
    {
        target = selection->TargetPtr[i];

        // CAT: MakeTargetsOnlyEnemyShips 0x00512970 also drops ships under KAS non-retaliation (specialFlags 0x40000
        //      in the exe = SPECIAL_FriendlyStatus, kasfShipsSetNonRetaliation 0x00495da0) and crystals; HW1 dropped Drones
        if ((target->header.objtype != OBJ_ShipType) ||
            (((Ship *)target)->playerowner == curplayer) ||
            bitTest(((Ship *)target)->shipOrDerelict.specialFlags, SPECIAL_FriendlyStatus) ||
            allianceIsShipAlly((Ship *)target, curplayer) ||
            cat_shipIsCrystal((Ship *)target))
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numTargets--;
            selection->TargetPtr[i] = selection->TargetPtr[selection->numTargets];

            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: MakeTargetsOnlyEnemyShips 0x00512970
}

/*-----------------------------------------------------------------------------
    Name        : MakeTargetsOnlyNonForceAttackTargets
    Description : Makes sure that the targets in this selection are ships and are enemies (not curplayer's ships)
    Inputs      : selection,curplayer
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeTargetsOnlyNonForceAttackTargets(SelectAnyCommand *selection,Player *curplayer)
{
    sdword i;
    SpaceObjRotImpTarg *target;
    Ship *ship;

    for (i=0;i<selection->numTargets;)
    {
        target = selection->TargetPtr[i];
        ship = (Ship *)target;

        // CAT: MakeTargetsOnlyNonForceAttackTargets 0x005129f0. Kept without question: another player's derelict-race
        //      ship (not a crystal) and the hyperspace gate. Enemy ships (not allied, not the Junkyard HQ) stay unless
        //      they are crystals, KAS derelicts, or mimics that look allied, like a resource or like a derelict to the
        //      current player. Mines stay. Everything else goes. HW1 kept KillerCollDamage objects and dropped Drones.
        if (((target->header.objtype == OBJ_ShipType) && (ship->staticinfo->shiprace == RACE_Derelict) &&
             (ship->playerowner != curplayer) && (!cat_shipIsCrystal(ship))) ||
            ((target->header.objtype == OBJ_DerelictType) && (((Derelict *)target)->derelicttype == HyperspaceGate)))
        {
            goto dontremove;
        }

        if ((target->header.objtype == OBJ_ShipType) && (!allianceIsShipAlly(ship, curplayer)) &&
            (ship->shiptype != trJunkYardHQ))
        {
            if (cat_shipIsCrystal(ship) || bitTest(ship->catShipFlags, CAT_SHIPFLAG_KasDerelict))
            {
                goto remove;
            }
            if ((ship->playerowner == universe.curPlayerPtr) || (!bitTest(ship->catShipFlags, CAT_SHIPFLAG_Mimic)) ||
                ((!cat_mimicAppearsAlliedTo(ship, universe.curPlayerPtr)) &&
                 (!cat_mimicAppearsAsResource(ship, universe.curPlayerPtr)) &&
                 (!cat_mimicAppearsAsDerelict(ship, universe.curPlayerPtr))))
            {
                goto dontremove;
            }
            goto remove;
        }

        if ((target->header.objtype == OBJ_MissileType) && (((Missile *)target)->missileType == MISSILE_Mine))
        {
            goto dontremove;
        }

remove:
        // to remove ShipPtr from list, decrement numShips and put last member
        // of array into slot being deleted.
        selection->numTargets--;
        selection->TargetPtr[i] = selection->TargetPtr[selection->numTargets];

        continue; // we must check same index again because we put last array entry here
dontremove:
        i++;
    }
    ioUpdateShipTotals();       // CAT: MakeTargetsOnlyNonForceAttackTargets 0x005129f0
}

/*-----------------------------------------------------------------------------
    Name        : MakeTargetsSalvageable
    Description : Filters the targets for any ship or derelict that's salvageable
    Inputs      : selection - the targets to filter,
                  curplayer - determines while player is enemy
    Outputs     : Changes selection
    Return      : void
----------------------------------------------------------------------------*/
void MakeTargetsSalvageable(SelectAnyCommand *selection, Player *curplayer)
{
    udword i;
    SpaceObjRotImpTarg *target;

    // CAT: the exe has no MakeTargetsSalvageable (its salvage targeting is cat_salvageTargetAllowed 0x00513920);
    //      kept for its unported HW1 callers (mainrgn.c: stage 3). HW1's drone and cryo-tray/Mission-1 exceptions
    //      are gone: Cataclysm has neither type.
    for (i=0;i<selection->numTargets;)
    {
        target = selection->TargetPtr[i];

        if (target->header.objtype == OBJ_DerelictType)
        {
            if (!((DerelictStaticInfo *)target->staticinfo)->salvageable)
            {
                selection->numTargets--;
                selection->TargetPtr[i] = selection->TargetPtr[selection->numTargets];
                continue;
            }
        }
        //here's what's happening:
        //if it's a ship and it's the player's or an ally's (and not disabled), it can't be salvaged
        else if (target->header.objtype == OBJ_ShipType)
        {
            Ship* ship = (Ship*)target;
            if (( (ship->playerowner == curplayer) ||
                  allianceIsShipAlly((Ship *)target, curplayer) )
                && !bitTest(ship->header.flags, SOF_Disabled))
            {
                selection->numTargets--;
                selection->TargetPtr[i] = selection->TargetPtr[selection->numTargets];
                continue;
            }
        }
        else
        {
            selection->numTargets--;
            selection->TargetPtr[i] = selection->TargetPtr[selection->numTargets];
            continue;
        }

        i++;
    }
}



/*-----------------------------------------------------------------------------
    Name        : MakeSelectionNotHaveCloakedEnemyShips
    Description : Removes enemy cloaked ships from the selection
    Inputs      : selection - the selection being filtered,
                  curplayer - the current player
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void MakeSelectionNotHaveCloakedEnemyShips(SelectCommand *selection, Player *curplayer)
{
    sdword i;
    ShipPtr ship;

    for (i=0;i<selection->numShips;)
    {
        ship = selection->ShipPtr[i];

        if ((ship->playerowner != curplayer) && (bitTest(ship->header.flags, SOF_Cloaked)))
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];

            continue;// we must check same index again because we put last array entry here
        }
        i++;
    }
}

/*-----------------------------------------------------------------------------
    Name        : makeShipsNotHaveNonCombatShipsForGuardAttack
    Description : Removes CloakGen...and other related ships
                    so that selection can be given an ATATCK order
                    while guarding and such that the cloak gen and other
                    ships are left guarding
    Inputs      : selection - the selection being filtered,
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/

void makeShipsNotHaveNonCombatShipsForGuardAttack(SelectCommand *selection)
{
    sdword i;
    ShipPtr ship;

    for (i=0;i<selection->numShips;)
    {
        ship = selection->ShipPtr[i];

        // CAT: the Kushan and Taiidan cloak and gravwell generators, the Taiidan DFG frigate and the Beast DFG frigate
        //      (makeShipsNotHaveNonCombatShipsForGuardAttack 0x00512ca0: types 0x3f, 0x40, 0x4d, 0x4f, 0x2d, 0x4e)
        if (ship->shiptype == kCloakGenerator ||
            ship->shiptype == kGravWellGenerator ||
            ship->shiptype == tCloakGenerator ||
            ship->shiptype == tGravWellGenerator ||
            ship->shiptype == bDFGFrigate ||
            ship->shiptype == tDFGFrigate)
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];

            continue;// we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: makeShipsNotHaveNonCombatShipsForGuardAttack 0x00512ca0
}


/*-----------------------------------------------------------------------------
    Name        : MakeSelectionNotHaveNonVisibleEnemyShips
    Description : Removes non-visible enemy ships from the selection
                  Note: takes into account Proximity Sensors detecting cloaked ships
    Inputs      : selection - the selection being filtered,
                  curplayer - the current player
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void MakeSelectionNotHaveNonVisibleEnemyShips(SelectCommand *selection, Player *curplayer)
{
    sdword i;
    ShipPtr ship;

    for (i=0;i<selection->numShips;)
    {
        ship = selection->ShipPtr[i];

        // CAT: fully cloaked enemy ships that curplayer has not detected go (MakeSelectionNotHaveNonVisibleEnemyShips
        //      0x00512d00: cat_shipIsCloakedFully, sub_53F9F0 = cat_shipDetectedByPlayer); HW1 asked the proximity sensors
        if ((ship->playerowner != curplayer) &&
            cat_shipIsCloakedFully(ship) &&
            (!cat_shipDetectedByPlayer(ship, curplayer->playerIndex)))
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];

            continue;// we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: MakeSelectionNotHaveNonVisibleEnemyShips 0x00512d00
}



/*-----------------------------------------------------------------------------
    Name        : MakeTargetsOnlyBeWithinRangeAndNotIncludeMe
    Description : Makes targets all be within range of comparewith, but don't include comparewith
    Inputs      : selection, comparewith, range
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeTargetsOnlyBeWithinRangeAndNotIncludeMe(SelectAnyCommand *selection,SpaceObjRotImpTarg *comparewith,real32 range)
{
    sdword i;
    SpaceObjRotImpTarg *target;
    real32 negrange = -range;
    vector diff;

    for (i=0;i<selection->numTargets;)
    {
        target = selection->TargetPtr[i];

        vecSub(diff,target->obj.posinfo.position,comparewith->obj.posinfo.position);
        if ((target == comparewith) ||
            !isBetweenExclusive(diff.x,negrange,range) ||
            !isBetweenExclusive(diff.y,negrange,range) ||
            !isBetweenExclusive(diff.z,negrange,range) )
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numTargets--;
            selection->TargetPtr[i] = selection->TargetPtr[selection->numTargets];

            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: MakeTargetsOnlyBeWithinRangeAndNotIncludeMe 0x00512d80
}

/*-----------------------------------------------------------------------------
    Name        : MakeShipsFriendlyShips
    Description : Makes sure that the ships in this selection are enemies (not curplayer's ships)
                  Removes any friendly ships from the selection
    Inputs      : selection,curplayer
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeShipsFriendlyShips(SelectCommand *selection,Player *curplayer)
{
    ShipPtr ship;
    sdword i;

    for (i=0;i<selection->numShips;)
    {
        ship = selection->ShipPtr[i];

        if ((ship->header.objtype != OBJ_ShipType) ||
            (ship->playerowner != curplayer) )
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];

            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: MakeShipsFriendlyShips 0x00512e50
}

/*-----------------------------------------------------------------------------
    Name        : MakeShipsFriendlyAndAlliesShips
    Description : Makes sure that the ships in this selection are curplayer's and allies  ships
                  Removes any enemy ships from the selection
    Inputs      : selection,curplayer
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeShipsFriendlyAndAlliesShips(SelectCommand *selection,struct Player *curplayer)
{
    sdword i;

    for (i=0;i<selection->numShips;)
    {
        if  ((selection->ShipPtr[i]->header.objtype != OBJ_ShipType)
            || ( ((selection->ShipPtr[i])->playerowner != curplayer) &&
               (!allianceIsShipAlly(selection->ShipPtr[i], curplayer)) ) )
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];

            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: MakeShipsFriendlyAndAlliesShips 0x00512ea0
}

/*-----------------------------------------------------------------------------
    Name        : makeshipMastersincludeslaves(SelectCommand *selection)
    Description : Checks the list of selected ships and if it finds a Master in the list,
                  it adds the slaves aswell.
    Inputs      : selection
    Outputs     : none
    Return      : none
----------------------------------------------------------------------------*/
void MakeShipMastersIncludeSlaves(SelectCommand *selection)
{
    sdword i,count;
    Node *slavenode;
    count = selection->numShips;

    for (i=0;i < count;)
    {
        if ((selection->ShipPtr[i])->header.flags & SOF_Slaveable)
        {   //ship is a most likely a master...
            if ((selection->ShipPtr[i])->slaveinfo->flags & SF_MASTER)
            {
                slavenode = (selection->ShipPtr[i])->slaveinfo->slaves.head;
                while(slavenode != NULL)
                {
                    if(selection->numShips < COMMAND_MAX_SHIPS)
                    {   //haven't reached maximum  so add slave onto end
                        selection->ShipPtr[selection->numShips] = (Ship *)listGetStructOfNode(slavenode);
                        selection->numShips++;
                    }
                    else
                    {   //reached max so break.
                        break;
                    }
                    slavenode = slavenode->next;
                }
            }
        }
        i++;
    }
}

/*-----------------------------------------------------------------------------
    Name        : selectAllCurrentPlayersNonHyperspacingShips
    Description : Selects all of the current player's non-hyperspacing ships
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
SelectCommand *selectAllCurrentPlayersNonHyperspacingShips(void)
{
    SelectCommand *selection;
    sdword i = 0;
    Node *objnode;
    Ship *ship;

    selection = memAlloc(sizeofSelectCommand(universe.ShipList.num),"selectall",0);

    i = 0;
    objnode = universe.ShipList.head;
    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        // CAT: the ships that may make the single-player hyperspace jump and do not go into it themselves: not
        //      capital, not components (selectAllCurrentPlayersNonHyperspacingShips 0x00512f00, cat_spShipCanHyperspace
        //      0x00517650 holds the ally/dead/disabled tests HW1 made here)
        if (cat_spShipCanHyperspace(ship) && (!ship->staticinfo->shipIsCapital) &&
            (ship->staticinfo->shipclass != CLASS_Component))
        {
            selection->ShipPtr[i++] = ship;
        }

        objnode = objnode->next;
    }

    selection->numShips = i;

    ioUpdateShipTotals();       // CAT: selectAllCurrentPlayersNonHyperspacingShips 0x00512f00
    return selection;
}

/*-----------------------------------------------------------------------------
    Name        : selectAllCurrentPlayersHyperspacingShips
    Description : Selects all of the current player's hyperspacing ships
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
SelectCommand *selectAllCurrentPlayersHyperspacingShips(void)
{
    SelectCommand *selection;
    sdword i = 0;
    Node *objnode;
    Ship *ship;

    selection = memAlloc(sizeofSelectCommand(universe.ShipList.num),"selectall",0);

    i = 0;
    objnode = universe.ShipList.head;
    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        // CAT: capital ships and components that may make the jump (selectAllCurrentPlayersHyperspacingShips
        //      0x00512f80); HW1 used ShipCanHyperspace and its own ally/disabled tests
        if (cat_spShipCanHyperspace(ship) &&
            ((ship->staticinfo->shipIsCapital) || (ship->staticinfo->shipclass == CLASS_Component)))
        {
            selection->ShipPtr[i++] = ship;
        }

        objnode = objnode->next;
    }

    selection->numShips = i;

    ioUpdateShipTotals();       // CAT: selectAllCurrentPlayersHyperspacingShips 0x00512f80
    return selection;
}

/*-----------------------------------------------------------------------------
    Name        : selectAllPlayersShips
    Description : selects all of a player's ships
    Inputs      : player
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
SelectCommand *selectAllPlayersShips(struct Player *player)
{
    SelectCommand *selection;
    sdword i = 0;
    Node *objnode;
    Ship *ship;

    selection = memAlloc(sizeofSelectCommand(universe.ShipList.num),"selectall",0);

    i = 0;
    objnode = universe.ShipList.head;
    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (ship->playerowner == player)
        {
            selection->ShipPtr[i++] = ship;
        }

        objnode = objnode->next;
    }

    selection->numShips = i;

    return selection;
}

/*-----------------------------------------------------------------------------
    Name        : MakeSelectionNonResearchShips(SelectCommand *selectcom)
    Description : Removes all research ship types from the selection
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

void MakeSelectionNonResearchShips(SelectCommand *selectcom)
{
    // CAT: Cataclysm has no research ships (the exe has no MakeSelectionNonResearchShips), so nothing is removed.
    //      Kept for its unported HW1 caller (CommandWrap.c: M2-R4 #2).
    (void)selectcom;
}

/*-----------------------------------------------------------------------------
    Name        : MakeShipsSingleClickSpecialCapable
    Description : Makes a selection of just ships that are have the
                    canSingleClickSpecialActivate flag
    Inputs      : source - selection to search through
    Outputs     : dest - where to store the culled selection
    Return      : number of ships in culled selection
----------------------------------------------------------------------------*/
sdword MakeShipsSingleClickSpecialCapable(SelectCommand *dest, SelectCommand *source)
{
/*
    sdword index;

    dest->numShips = 0;
    for (index = 0; index < source->numShips; index++)
    {
        if (source->ShipPtr[index]->staticinfo->canSingleClickSpecialActivate)
        {
            dest->ShipPtr[dest->numShips] = source->ShipPtr[index];
            dest->numShips++;
        }
    }
    return(dest->numShips);
*/
    sdword index, numShips = 0;

    for (index = 0; index < source->numShips; index++)
    {
        if (source->ShipPtr[index]->staticinfo->canSingleClickSpecialActivate)
        {
            dest->ShipPtr[numShips] = source->ShipPtr[index];
            numShips++;
        }
    }
    dest->numShips = numShips;
    return(numShips);
}

/*-----------------------------------------------------------------------------
    Name        : MakeShipsNotDocked
    Description : Makes a selection of ships that are not hiddel
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
sdword MakeShipsNotHidden(SelectCommand *dest, SelectCommand *source)
{
    sdword index, numShips = 0;

    for (index = 0; index < source->numShips; index++)
    {
        if (!bitTest(source->ShipPtr[index]->header.flags, SOF_Hide))
        {
            dest->ShipPtr[numShips] = source->ShipPtr[index];
            numShips++;
        }
    }
    dest->numShips = numShips;
    ioUpdateShipTotals();       // CAT: MakeShipsNotHidden 0x00513010
    return(numShips);
}

/*-----------------------------------------------------------------------------
    Name        : selectSelectionInterception
    Description : Returns a selection consisting of ships in both selection1 and selection2
    Inputs      : selection1, selection2
    Outputs     : Creates a new selection
    Return      : the new selection, NULL if there's no intersection
----------------------------------------------------------------------------*/
SelectCommand *selectSelectionIntersection(SelectCommand *selection1, SelectCommand *selection2)
{
    SelectCommand *smallerSelection, *largerSelection;
    SelectCommand *intersection = NULL;
    ShipPtr ship;
    sdword i;

    if (selection1->numShips < selection2->numShips)
    {
        smallerSelection = selection1;
        largerSelection  = selection2;
    }
    else
    {
        smallerSelection = selection2;
        largerSelection  = selection1;
    }

    intersection = selectDupSelection(smallerSelection);

    for (i=intersection->numShips-1; i >= 0; i--)
    {
        ship = intersection->ShipPtr[i];

        if (!ShipInSelection(largerSelection, ship))
        {
            clRemoveShipFromSelection(intersection, ship);
        }
    }

    if (!intersection->numShips)
    {
        memFree(intersection);
        intersection = NULL;
    }

    return intersection;
}




/*-----------------------------------------------------------------------------
    Name        : selectMergeTwoSelections
    Description : Takes two selections and makes one big one. Note that a new selection
                  is allocated in memory.
    Inputs      : selection1, selection2 - the selections to merge
    Outputs     : Allocates memory space for a new selection, frees the other two spaces
    Return      : The new selection
----------------------------------------------------------------------------*/
SelectCommand *selectMergeTwoSelections(SelectCommand *selection1, SelectCommand *selection2, udword dealloc)
{
    SelectCommand *new_selection;
    udword i, j;

    if (selection1 == NULL)
    {
        //if selection1 is NULL, copy selection2 into the new selection
        new_selection = (SelectCommand *)memAlloc(sizeofSelectCommand(selection2->numShips), "merge_sel2", 0);
        new_selection->numShips = selection2->numShips;

        for (i = 0; i < selection2->numShips; i++)
        {
            new_selection->ShipPtr[i] = selection2->ShipPtr[i];
        }
        if ((dealloc == DEALLOC2) || (dealloc == DEALLOC_BOTH))
        {
            memFree(selection2);
        }
    }
    else if (selection2 == NULL)
    {
        //if selection2 is NULL, copy selection1 into the new selection
        new_selection = (SelectCommand *)memAlloc(sizeofSelectCommand(selection1->numShips), "merge_sel1", 0);
        new_selection->numShips = selection1->numShips;

        for (i = 0; i < selection1->numShips; i++)
        {
            new_selection->ShipPtr[i] = selection1->ShipPtr[i];
        }
        if ((dealloc == DEALLOC1) || (dealloc == DEALLOC_BOTH))
        {
            memFree(selection1);
        }
    }
    else
    {
        MakeShipsNotIncludeTheseShips(selection1, selection2);

        new_selection = (SelectCommand *)memAlloc(sizeofSelectCommand(selection1->numShips + selection2->numShips), "merge_sel", 0);

        for (i = 0; i < selection1->numShips; i++)
        {
            new_selection->ShipPtr[i] = selection1->ShipPtr[i];
        }
        for (j = 0; j < selection2->numShips; j++, i++)
        {
            new_selection->ShipPtr[i] = selection2->ShipPtr[j];
        }

        new_selection->numShips = i;

        switch (dealloc)
        {
            case DEALLOC1:
                memFree(selection1);
                break;
            case DEALLOC2:
                memFree(selection2);
                break;
            case DEALLOC_BOTH:
                memFree(selection1);
                memFree(selection2);
                break;
            default:
                break;
        }
    }

    return new_selection;
}


/*-----------------------------------------------------------------------------
    Name        : selectMemDupSelection
    Description : Creates a new selection that is an exact duplicate of another selection
    Inputs      : selection - the selection to duplicate
                  flag - the memory allocation flag
    Outputs     : Allocates memory for the new selection
    Return      : The new selection
----------------------------------------------------------------------------*/
SelectCommand *selectMemDupSelection(SelectCommand *selection, char *str, udword mmflag)
{
    SelectCommand *new_selection;

    new_selection = (SelectCommand *)memAlloc(sizeofSelectCommand(selection->numShips), str, mmflag);

    memcpy(new_selection,selection,sizeofSelectCommand(selection->numShips));

    return new_selection;
}

/*-----------------------------------------------------------------------------
    Name        : MothershipOrCarrierIndexInSelection
    Description : returns index where a mothership or carrier is in selection.  -1 if none found.
                  motherships have higher priority
    Inputs      :
    Outputs     :
    Return      : returns index where a mothership or carrier is in selection.  -1 if none found
----------------------------------------------------------------------------*/
sdword MothershipOrCarrierIndexInSelection(SelectCommand *selection)
{
    sdword numShips = selection->numShips;
    sdword index = -1;
    sdword i;
    ShipClass shipclass;

    // go through selection to find index where a mothership or carrier is.  Give higher priority to motherships.
    // CAT: by class (MothershipOrCarrierIndexInSelection 0x00513310: shipclass 0 returns, shipclass 2 is remembered);
    //      HW1 tested its mothership and carrier types
    for (i=0;i<numShips;i++)
    {
        shipclass = selection->ShipPtr[i]->staticinfo->shipclass;
        if (shipclass == CLASS_Mothership)
        {
            return i;
        }
        else if (shipclass == CLASS_Carrier)
        {
            index = i;
        }
    }

    return index;
}

/*=============================================================================
    Grow select functions
=============================================================================*/

void growSelectAddShip(GrowSelection *growSelect,Ship *ship)
{
    // reserve more space if required
    // CAT: the exe grows the selection in place with memRealloc (growSelectAddShip 0x00513350); HW1 allocated a new
    //      block, copied and freed the old one
    if (growSelect->selection->numShips >= growSelect->maxNumShips)
    {
        growSelect->maxNumShips += GROWSELECT_ADDBATCH;
        growSelect->selection = memRealloc(growSelect->selection, sizeofSelectCommand(growSelect->maxNumShips), "regrowships", 0);
    }

    growSelect->selection->ShipPtr[growSelect->selection->numShips] = ship;
    growSelect->selection->numShips += 1;
}

bool32 growSelectRemoveShip(GrowSelection *growSelect,Ship *ship)
{
    return clRemoveShipFromSelection(growSelect->selection,ship);
}

void growSelectRemoveShipIndex(GrowSelection *growSelect,sdword index)
{
    SelectCommand *selection = growSelect->selection;
    dbgAssertOrIgnore(index < selection->numShips);
    selection->numShips--;
    selection->ShipPtr[index] = selection->ShipPtr[selection->numShips];
}

void growSelectInit(GrowSelection *growSelect)
{
    growSelect->maxNumShips = GROWSELECT_INITIALBATCH;
    growSelect->selection = memAlloc(sizeofSelectCommand(GROWSELECT_INITIALBATCH), "growships", 0);
    growSelect->selection->numShips = 0;
}

void growSelectClose(GrowSelection *growSelect)
{
    if (growSelect->selection)
    {
        memFree(growSelect->selection);
        growSelect->selection = NULL;
    }
    growSelect->maxNumShips = 0;
}

void growSelectReset(GrowSelection *growSelect)
{
    growSelect->selection->numShips = 0;
}

void growSelectAddShipNoDuplication(GrowSelection *growSelect, Ship *ship)
{
    if (!ShipInSelection(growSelect->selection, ship))
    {
        growSelectAddShip(growSelect, ship);
    }
}


bool32 MakeSelectionKamikazeCapable(SelectCommand *selection)
{
    sdword i;

    for (i = 0; i < selection->numShips; )
    {
        // CAT: every resource-class ship may kamikaze (MakeSelectionKamikazeCapable 0x005134e0 keeps classes 6, 5, 7);
        //      HW1 kept its resource-collector type
        if(selection->ShipPtr[i]->staticinfo->shipclass != CLASS_Fighter &&
           selection->ShipPtr[i]->staticinfo->shipclass != CLASS_Corvette &&
           selection->ShipPtr[i]->staticinfo->shipclass != CLASS_Resource)
        {
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
            continue;
        }
        i++;
    }
    ioUpdateShipTotals();       // CAT: MakeSelectionKamikazeCapable 0x005134e0
    if(selection->numShips <= 0)
        return FALSE;
    return TRUE;
}

SelectCommand * shipLinkedListAsSelectCommand(LinkedList *list, char *label)
{
    Node *objnode = NULL;
    SelectCommand *selection = NULL;
    Ship *ship = NULL;
    udword i = 0;

    selection = memAlloc(sizeofSelectCommand(list->num), (label == NULL ? "" : label), 0);
    objnode = list->head;

    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        selection->ShipPtr[i++] = ship;

        objnode = objnode->next;
    }

    selection->numShips = i;

    return selection;
}

/*=============================================================================
    CAT: Cataclysm-only selection functions. The exe keeps them in its ShipSelect.c block
    (0x00511870-0x00514210); each carries its exe name and address.
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : cat_selSelectionAddShip
    Description : CAT (0x00511870): appends ship to a MaxSelection if there is room
----------------------------------------------------------------------------*/
void cat_selSelectionAddShip(MaxSelection *dest, Ship *ship)
{
    if (dest->numShips < COMMAND_MAX_SHIPS)
    {
        dest->ShipPtr[dest->numShips] = ship;
        dest->numShips++;
    }
    // the exe calls an empty debug print when the selection is full
}

/*-----------------------------------------------------------------------------
    Name        : cat_selSelectionCopy
    Description : CAT (0x005139b0): copies at most 255 ship pointers, but takes src's count, as the exe does
----------------------------------------------------------------------------*/
void cat_selSelectionCopy(MaxSelection *dest, MaxSelection *src)
{
    sdword i;

    for (i = 0; (i < src->numShips) && (i < 255); i++)
    {
        dest->ShipPtr[i] = src->ShipPtr[i];
    }
    dest->numShips = src->numShips;
}

/*-----------------------------------------------------------------------------
    Name        : cat_growSelectNullShip
    Description : CAT (0x005133e0): replaces the first occurrence of ship in grow with NULL (count unchanged)
----------------------------------------------------------------------------*/
void cat_growSelectNullShip(GrowSelection *grow, Ship *ship)
{
    SelectCommand *selection = grow->selection;
    sdword i;

    if (selection != NULL)
    {
        for (i = 0; i < selection->numShips; i++)
        {
            if (selection->ShipPtr[i] == ship)
            {
                selection->ShipPtr[i] = NULL;
                return;
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsScuttleable
    Description : CAT (0x00511ce0): with check, removes disabled ships and the Sect/Beast carriers and motherships
                  (the latter with a "cannot comply" chatter from the first selected ship)
----------------------------------------------------------------------------*/
void cat_selMakeShipsScuttleable(SelectCommand *selection, bool32 check)
{
    sdword i;
    Ship *ship;

    for (i = 0; i < selection->numShips; )
    {
        ship = selection->ShipPtr[i];
        if (check)
        {
            if (ship->catDisabled)
            {
                selection->numShips--;
                selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
                continue;
            }
            if ((ship->shiptype == sCarrier) || (ship->shiptype == sMothership) ||
                (ship->shiptype == bCarrier) || (ship->shiptype == bMothership))
            {
                if (battleCanChatterAtThisTime(BCE_CannotComply, selSelected.ShipPtr[0]))
                {
                    battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_CannotComply, selSelected.ShipPtr[0], selSelected.ShipPtr[0]->shiptype);
                }
                selection->numShips--;
                selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
                continue;
            }
        }
        i++;
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsNotLinkedACVs
    Description : CAT (0x00511df0): removes Sect/Beast ACVs whose link state is 1 or 2
----------------------------------------------------------------------------*/
void cat_selMakeShipsNotLinkedACVs(SelectCommand *selection)
{
    sdword i;
    Ship *ship;
    sdword linkState;

    for (i = 0; i < selection->numShips; )
    {
        ship = selection->ShipPtr[i];
        if ((ship->shiptype == sACV) || (ship->shiptype == bACV))
        {
            linkState = cat_acvLinkState(ship);
            if ((linkState == 1) || (linkState == 2))
            {
                selection->numShips--;
                selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
                continue;
            }
        }
        i++;
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsSameOwnerAsFirst
    Description : CAT (0x00511e50): keeps the ships owned by the first ship's owner
----------------------------------------------------------------------------*/
void cat_selMakeShipsSameOwnerAsFirst(SelectCommand *selection)
{
    sdword i;

    for (i = 0; i < selection->numShips; )
    {
        if (selection->ShipPtr[i]->playerowner != selection->ShipPtr[0]->playerowner)
        {
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
            continue;
        }
        i++;
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsHaltable
    Description : CAT (0x00511ea0): with check, removes disabled ships unless they are upgrading
----------------------------------------------------------------------------*/
void cat_selMakeShipsHaltable(SelectCommand *selection, bool32 check)
{
    sdword i;
    Ship *ship;

    for (i = 0; i < selection->numShips; )
    {
        ship = selection->ShipPtr[i];
        if (check && ship->catDisabled && (!ship->catUpgrading))
        {
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
            continue;
        }
        i++;
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsProtectCapable
    Description : CAT (0x00511f00): removes (with check) disabled ships and ships whose current order cannot be
                  replaced by a guard order
----------------------------------------------------------------------------*/
void cat_selMakeShipsProtectCapable(SelectCommand *selection, bool32 check)
{
    sdword i;
    Ship *ship;
    bool32 remove;

    for (i = 0; i < selection->numShips; )
    {
        ship = selection->ShipPtr[i];
        remove = FALSE;
        if (check && ship->catDisabled)
        {
            remove = TRUE;
        }
        else if (ship->command != NULL)
        {
            switch (ship->command->ordertype.order)
            {
                case COMMAND_LAUNCH_SHIP:
                case COMMAND_BUILDING_SHIP:
                case COMMAND_MP_HYPERSPACING:
                case CAT_COMMAND_11:
                case CAT_COMMAND_12:
                case CAT_COMMAND_14:
                case CAT_COMMAND_15:
                case CAT_COMMAND_16:
                case CAT_COMMAND_17:
                    remove = TRUE;
                    break;
                default:
                    break;
            }
        }
        if (remove)
        {
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
            continue;
        }
        i++;
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsMobileNotBusy
    Description : CAT (0x00511f90): MakeShipsMobile with the disabled-ship test always on (the exe body is a copy of
                  MakeShipsMobile 0x005126b0 with the argument forced)
----------------------------------------------------------------------------*/
void cat_selMakeShipsMobileNotBusy(SelectCommand *selection)
{
    MakeShipsMobile(selection, TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsHaveAbilityUpgrade
    Description : CAT (0x00512330): keeps the ships with any of the three ability upgrades
----------------------------------------------------------------------------*/
void cat_selMakeShipsHaveAbilityUpgrade(SelectCommand *selection)
{
    sdword i;
    Ship *ship;

    for (i = 0; i < selection->numShips; )
    {
        ship = selection->ShipPtr[i];
        if ((!cat_shipHasUpgrade(ship, CAT_UPGRADE_Ability1)) &&
            (!cat_shipHasUpgrade(ship, CAT_UPGRADE_Ability2)) &&
            (!cat_shipHasUpgrade(ship, CAT_UPGRADE_Ability3)))
        {
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
            continue;
        }
        i++;
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsLeechCapable
    Description : CAT (0x005123a0): keeps the ships cat_shipCanLeech accepts
----------------------------------------------------------------------------*/
void cat_selMakeShipsLeechCapable(SelectCommand *selection, bool32 check)
{
    sdword i;

    for (i = 0; i < selection->numShips; )
    {
        if (!cat_shipCanLeech(selection->ShipPtr[i], check))
        {
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
            continue;
        }
        i++;
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsRammingFrigates
    Description : CAT (0x00512410): keeps the Sect/Beast ramming frigates (with check: not disabled)
----------------------------------------------------------------------------*/
void cat_selMakeShipsRammingFrigates(SelectCommand *selection, bool32 check)
{
    sdword i;
    Ship *ship;

    for (i = 0; i < selection->numShips; )
    {
        ship = selection->ShipPtr[i];
        if (((ship->shiptype == sRammingFrigate) || (ship->shiptype == bRammingFrigate)) &&
            ((!check) || (!ship->catDisabled)))
        {
            i++;
            continue;
        }
        selection->numShips--;
        selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsRepairCapable
    Description : CAT (0x00512470): keeps the ships cat_shipCanRepair accepts (inlined in the exe)
----------------------------------------------------------------------------*/
void cat_selMakeShipsRepairCapable(SelectCommand *selection)
{
    sdword i;

    for (i = 0; i < selection->numShips; )
    {
        if (!cat_shipCanRepair(selection->ShipPtr[i]))
        {
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
            continue;
        }
        i++;
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsHaveUpgrade
    Description : CAT (0x005124e0): keeps the ships with any of the upgrade bits in flags
----------------------------------------------------------------------------*/
void cat_selMakeShipsHaveUpgrade(SelectCommand *selection, udword flags)
{
    sdword i;

    for (i = 0; i < selection->numShips; )
    {
        if (!cat_shipHasUpgrade(selection->ShipPtr[i], flags))
        {
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
            continue;
        }
        i++;
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsAcolyteLinkFire
    Description : CAT (0x00512530): keeps the ships that can fire the Acolyte linked weapon
----------------------------------------------------------------------------*/
void cat_selMakeShipsAcolyteLinkFire(SelectCommand *selection)
{
    sdword i;
    Ship *ship;

    for (i = 0; i < selection->numShips; )
    {
        ship = selection->ShipPtr[i];
        if ((ship->header.objtype == OBJ_ShipType) && cat_acolyteCanFireLinkedWeapon(ship))
        {
            i++;
            continue;
        }
        selection->numShips--;
        selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsNotBigShips
    Description : CAT (0x00512580): removes the big Sect and Beast ships
----------------------------------------------------------------------------*/
void cat_selMakeShipsNotBigShips(SelectCommand *selection)
{
    sdword i;
    ShipType type;

    for (i = 0; i < selection->numShips; )
    {
        type = selection->ShipPtr[i]->shiptype;
        if ((type == sMothership) || (type == sCarrier) || (type == sDreadnought) || (type == sDestroyer) ||
            (type == sProcessor) || (type == bMothership) || (type == bCarrier) || (type == bHeavyCruiser) ||
            (type == bProcessor))
        {
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
            continue;
        }
        i++;
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsNotBusy
    Description : CAT (0x005125f0): removes disabled ships, except Leeches and Sentinels
----------------------------------------------------------------------------*/
void cat_selMakeShipsNotBusy(SelectCommand *selection)
{
    sdword i;
    Ship *ship;

    for (i = 0; i < selection->numShips; )
    {
        ship = selection->ShipPtr[i];
        if ((!ship->catDisabled) || (ship->shiptype == sLeech) || (ship->shiptype == sSentinel) ||
            (ship->shiptype == bSentinel))
        {
            i++;
            continue;
        }
        selection->numShips--;
        selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsLeeching
    Description : CAT (0x00512650): keeps the leech-capable ships that are leeching
----------------------------------------------------------------------------*/
void cat_selMakeShipsLeeching(SelectCommand *selection)
{
    sdword i;

    for (i = 0; i < selection->numShips; )
    {
        if (cat_shipCanUnleech(selection->ShipPtr[i]))
        {
            i++;
            continue;
        }
        selection->numShips--;
        selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeTargetsAttackable
    Description : CAT (0x00512b50): removes the ships that cannot be attacked (untargetable types, Sect Acolytes,
                  dissolving or hyperspacing ships, the infected Ecclesiam, and outside single player without force
                  a Sentinel whose field is up) and dust clouds
----------------------------------------------------------------------------*/
void cat_selMakeTargetsAttackable(SelectCommand *targets, bool32 force)
{
    sdword i;
    Ship *ship;
    bool32 fieldActive;

    for (i = 0; i < targets->numShips; )
    {
        ship = targets->ShipPtr[i];
        if (ship->header.objtype == OBJ_ShipType)
        {
            fieldActive = FALSE;
            if ((!force) && (!singlePlayerGame) && ((ship->shiptype == sSentinel) || (ship->shiptype == bSentinel)))
            {
                fieldActive = cat_shipIsSentinelFieldActive(ship);
            }
            if (cat_shipIsUntargetable(ship) || (ship->shiptype == sAcolyte) || ship->catDissolving ||
                bitTest(ship->shipOrDerelict.specialFlags, SPECIAL_Hyperspacing) ||
                (ship->shiptype == dEcclesiamInfected) || fieldActive)
            {
                targets->numShips--;
                targets->ShipPtr[i] = targets->ShipPtr[targets->numShips];
                continue;
            }
        }
        else if (ship->header.objtype == OBJ_DustType)
        {
            targets->numShips--;
            targets->ShipPtr[i] = targets->ShipPtr[targets->numShips];
            continue;
        }
        i++;
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeTargetsBeastAttackable
    Description : CAT (0x00512c40): cat_selMakeTargetsAttackable without force, then removes crystals
----------------------------------------------------------------------------*/
void cat_selMakeTargetsBeastAttackable(SelectCommand *targets)
{
    sdword i;
    Ship *ship;

    cat_selMakeTargetsAttackable(targets, FALSE);
    for (i = 0; i < targets->numShips; )
    {
        ship = targets->ShipPtr[i];
        if ((ship->header.objtype == OBJ_ShipType) && cat_shipIsCrystal(ship))
        {
            targets->numShips--;
            targets->ShipPtr[i] = targets->ShipPtr[targets->numShips];
            continue;
        }
        i++;
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_selMakeShipsDockCapableFor
    Description : CAT (0x00513710): keeps the ships whose class fits through a slipgate of dockTargetType (with
                  check: not disabled); any other dockTargetType has gate size 0
----------------------------------------------------------------------------*/
void cat_selMakeShipsDockCapableFor(SelectCommand *selection, ShipType dockTargetType, bool32 check)
{
    sdword i;
    Ship *ship;

    for (i = 0; i < selection->numShips; )
    {
        ship = selection->ShipPtr[i];
        if (((!check) || (!ship->catDisabled)) &&
            cat_shipClassCanUseSlipgate(ship->staticinfo->shipclass, dockTargetType))
        {
            i++;
            continue;
        }
        selection->numShips--;
        selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
    }
    ioUpdateShipTotals();
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipClassCanUseSlipgate
    Description : CAT (0x00513800): small/medium/large slipgates (gate size 1/2/3) take ships of the class sizes
                  below; the exe inlines the same table in cat_selMakeShipsDockCapableFor 0x00513710
----------------------------------------------------------------------------*/
static udword cat_slipgateSize(ShipType slipgateType)
{
    if (slipgateType == dSlipgateSmall)  return 1;
    if (slipgateType == dSlipgateMedium) return 2;
    if (slipgateType == dSlipgateLarge)  return 3;
    return 0;
}

static udword cat_shipClassGateSizeNeeded(ShipClass shipclass)
{
    switch (shipclass)
    {
        case CLASS_Mothership:
        case CLASS_MonsterShip:
            return 3;
        case CLASS_HeavyCruiser:
        case CLASS_Carrier:
        case CLASS_Destroyer:
        case CLASS_Frigate:
        case CLASS_Resource:
            return 2;
        case CLASS_Corvette:
        case CLASS_Fighter:
        case CLASS_NonCombat:
        case CLASS_Component:
        case CLASS_Micro:
            return 1;
        default:
            return 4;
    }
}

bool32 cat_shipClassCanUseSlipgate(ShipClass shipclass, ShipType slipgateType)
{
    return (cat_slipgateSize(slipgateType) >= cat_shipClassGateSizeNeeded(shipclass));
}

/*-----------------------------------------------------------------------------
    CAT: ship capability predicates (0x00513540-0x005138f0)
----------------------------------------------------------------------------*/
bool32 cat_shipCanMove(Ship *ship, bool32 check)                    // 0x00513540
{
    if (ship->staticinfo->common.immobile)
    {
        return FALSE;
    }
    if (check && ship->catDisabled)
    {
        return FALSE;
    }
    return TRUE;
}

bool32 cat_shipCanDock(Ship *ship, bool32 check)                    // 0x00513570
{
    ShipClass shipclass = ship->staticinfo->shipclass;

    if ((shipclass == CLASS_Mothership) || (shipclass == CLASS_Component))
    {
        return FALSE;
    }
    if (ship->staticinfo->externalBuild)
    {
        return FALSE;
    }
    if (check && ship->catDisabled)
    {
        return FALSE;
    }
    return TRUE;
}

bool32 cat_shipCanBeHalted(Ship *ship, bool32 check)                // 0x005135c0
{
    if (check && ship->catDisabled)
    {
        return FALSE;
    }
    return TRUE;
}

bool32 cat_shipCanSetTactics(Ship *ship, bool32 check)              // 0x005135e0
{
    if (check && ship->catDisabled)
    {
        return FALSE;
    }
    if (ship->shiptype == sSwarmer)
    {
        return FALSE;
    }
    return !bitTest(ship->catShipFlags, CAT_SHIPFLAG_NoTactics);
}

bool32 cat_shipCanCollectResources(Ship *ship, bool32 check)        // 0x00513620
{
    if ((ship->staticinfo->shipclass != CLASS_Resource) || (ship->staticinfo->harvestAmount < 1))
    {
        return FALSE;
    }
    if ((!singlePlayerGame) && (!bitTest(tpGameCreated.flag, MG_HarvestinEnabled)))
    {
        return FALSE;
    }
    if (check && (ship->catDisabled || ((ship->shiptype != sWorker) && (ship->shiptype != bWorker))))
    {
        return FALSE;
    }
    return TRUE;
}

bool32 cat_shipCanLeech(Ship *ship, bool32 checkAttached)           // 0x005136a0
{
    if (!cat_shipHasUpgrade(ship, CAT_UPGRADE_Leech))
    {
        return FALSE;
    }
    if (checkAttached && ship->catDisabled && (!cat_leechIsLeeching(ship)))
    {
        return FALSE;
    }
    return TRUE;
}

bool32 cat_shipCanUnleech(Ship *ship)                               // 0x005136e0
{
    if (!cat_shipHasUpgrade(ship, CAT_UPGRADE_Leech))
    {
        return FALSE;
    }
    return cat_leechIsLeeching(ship) ? TRUE : FALSE;
}

bool32 cat_shipCanRepair(Ship *ship)                                // 0x005138b0
{
    return (ship->header.objtype == OBJ_ShipType) && cat_shipHasUpgrade(ship, CAT_UPGRADE_Repair) &&
           (ship->shiptype != sProcessor) && (ship->shiptype != bProcessor);
}

bool32 cat_shipCanSalvage(Ship *ship)                               // 0x005138f0
{
    return (ship->header.objtype == OBJ_ShipType) && cat_shipHasUpgrade(ship, CAT_UPGRADE_Salvage);
}

/*-----------------------------------------------------------------------------
    CAT: ship upgrades (0x00513f40-0x00514210). The flags live in Ship.catUpgradeFlags; the player-side upgrade
    state (cat_shipUpgradesInit 0x005140a0, cat_playerUpgradesReset 0x00514100,
    cat_playerUpdateUpgradesFromResearch 0x00514140) is ported with the research code (M2-R4 #5).
----------------------------------------------------------------------------*/
bool32 cat_shipHasUpgrade(Ship *ship, udword flag)                  // 0x00514210
{
    return (ship->catUpgradeFlags & flag) != 0;
}

real32 cat_upgradeFlagsBaseTime(udword flags)                       // 0x00513f40
{
    // 10 seconds per upgrade bit, 20 for 0x10000, in the exe's order; bits 0x1000000 and 0x8000000 and up cost nothing
    static const udword tenSecondBits[] =
    {
        0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000
    };
    static const udword tenSecondBitsAfter[] =
    {
        0x20000, 0x40000, 0x200000, 0x80000, 0x100000, 0x400000, 0x800000, 0x4000000, 0x2000000
    };
    real32 time = 0.0f;
    udword i;

    if (flags & 0x1)
    {
        time = 10.0f;
    }
    for (i = 0; i < sizeof(tenSecondBits) / sizeof(tenSecondBits[0]); i++)
    {
        if (flags & tenSecondBits[i])
        {
            time += 10.0f;
        }
    }
    if (flags & 0x10000)
    {
        time += 20.0f;
    }
    for (i = 0; i < sizeof(tenSecondBitsAfter) / sizeof(tenSecondBitsAfter[0]); i++)
    {
        if (flags & tenSecondBitsAfter[i])
        {
            time += 10.0f;
        }
    }
    return time;
}

real32 cat_shipUpgradeFlagsTime(Ship *ship, udword flags)           // 0x00514080
{
    return cat_upgradeFlagsBaseTime(flags) * ship->staticinfo->upgradeTimeModifier;
}

/*=============================================================================
    CAT: ship-state queries the selection code calls. Their exe homes are other subsystems (address in each
    comment); they live here until the code that owns their state is ported, then move to their home files.
    Each one that reads unported ship-specific state says so: CAT-PROVISIONAL.
=============================================================================*/

bool32 cat_shipIsCrystal(Ship *ship)                                // 0x005affa0
{
    return (ship != NULL) && (ship->header.objtype == OBJ_ShipType) &&
           ((ship->shiptype == dCrystal0) || (ship->shiptype == dCrystal1));
}

// cat_shipIsCloakedFully 0x00436110 lives in Cloak.c with the rest of Cataclysm's cloak [M2-R3 ships-a]

bool32 cat_shipIsAcolyte(Ship *ship)                                // 0x005ad300 [M2-R3 ships-a]
{
    // sAcolyte and bAcolyte (not dAcolyte)
    return ((ship->shiptype == sAcolyte) || (ship->shiptype == bAcolyte)) ? TRUE : FALSE;
}

void cat_shipSetDisableReason(Ship *ship, sdword value, sdword reason)      // 0x005b3fe0 [M2-R3 ships-a]
{
    // one of 13 disable reasons; the ship is disabled while any of them is set
    sdword i;

    dbgAssertOrIgnore((reason >= 0) && (reason < CAT_NUM_DISABLE_REASONS));
    ship->catDisableReason[reason] = value;
    ship->catDisabled = FALSE;
    for (i = 0; i < CAT_NUM_DISABLE_REASONS; i++)
    {
        if (ship->catDisableReason[i] != 0)
        {
            ship->catDisabled = TRUE;
        }
    }
}

bool32 cat_shipIsUntargetable(Ship *ship)                           // 0x0052a060
{
    return (ship->staticinfo->catTypeFlags >> 14) & 1;
}

bool32 cat_shipIsSentinelInField(Ship *ship)                        // 0x005c6ec0
{
    return ((ship->shiptype == sSentinel) || (ship->shiptype == bSentinel)) && (ship->command != NULL) &&
           (ship->command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
           ((ship->command->formation.formationtype == CAT_FORMATION_10) ||
            (ship->command->formation.formationtype == CAT_FORMATION_9));
}

bool32 cat_shipIsSentinelFieldActive(Ship *ship)                    // 0x005c6e50
{
    // CAT-PROVISIONAL: the exe reads the Sentinel specifics (state +0x638 == 1 and field level +0x63C == 1.0); the
    //      Sentinel code is ported in M2-R4 #4 (ship code B). Until then no field is up.
    (void)ship;
    return FALSE;
}

bool32 cat_leechIsLeeching(Ship *ship)                              // 0x005b7ce0
{
    // CAT-PROVISIONAL: the exe reads the first byte of the Leech specifics (Ship+0x638); the Leech code is ported in
    //      M2-R4 #4 (ship code B). Until then no ship is leeching.
    (void)ship;
    return FALSE;
}

sdword cat_acvLinkState(Ship *ship)
{
    // CAT-PROVISIONAL: the exe reads the ACV specifics byte at Ship+0x6F0 (cat_selMakeShipsNotLinkedACVs 0x00511df0);
    //      the ACV link code is ported in M2-R4 #4 (ship code B). Until then no ACV is linked (state 0).
    (void)ship;
    return 0;
}

bool32 cat_acolyteCanFireLinkedWeapon(Ship *ship)                   // 0x005ada50
{
    // the third or fourth gun (the linked weapon) must hold missiles; the exe's Gun has a leading upgrade-level
    // dword, so its +0x68 is numMissiles
    return ((ship->shiptype == sAcolyte) || (ship->shiptype == bAcolyte)) &&
           cat_shipHasUpgrade(ship, CAT_UPGRADE_AcolyteLinkFire) &&
           ((ship->gunInfo->guns[2].numMissiles > 0) || (ship->gunInfo->guns[3].numMissiles > 0));
}

bool32 cat_salvageIsBeingSalvaged(Ship *ship)                       // 0x004f7c00
{
    return bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingSalvaged) ? TRUE : FALSE;
}

bool32 cat_salvageDepotHoldsCrystal(Ship *ship)
{
    // CAT-PROVISIONAL: the exe scans the 16 salvage depot slots (5 dwords each) of cat_salvageGetDepotInfo 0x004f91c0
    //      for a crystal (makeSelectionHyperspaceCapable 0x005120d0); the depots live in the specifics of the
    //      carriers, motherships and processors and are ported with the salvage system (M2-R4 #3). Until then no
    //      depot holds anything.
    (void)ship;
    return FALSE;
}

bool32 cat_mothershipBigGunActive(Ship *ship)                       // 0x005bbf60
{
    SelectCommand *clamped;
    bool32 bigGunReady = FALSE;
    sdword i;

    if ((ship->shiptype != sMothership) || (ship->guidance.clampInfo == NULL) ||
        (ship->guidance.clampInfo->clampedObjs == NULL))
    {
        return FALSE;
    }
    clamped = (SelectCommand *)ship->guidance.clampInfo->clampedObjs;
    for (i = 0; i < clamped->numShips; i++)
    {
        if ((clamped->ShipPtr[i]->shiptype == sMothershipBigGun) &&
            (!bitTest(clamped->ShipPtr[i]->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt)))
        {
            bigGunReady = TRUE;
        }
    }
    // CAT-PROVISIONAL: the exe also needs the mothership specifics byte at Ship+0x674 (big gun firing); the Sect
    //      mothership code is ported in M2-R4 #4 (ship code B). Until then the big gun never fires.
    (void)bigGunReady;
    return FALSE;
}

bool32 cat_shipDetectedByPlayer(Ship *ship, sdword playerIndex)     // sub_53F9F0 0x0053f9f0
{
    // Ship types with the stealth type flag (catTypeFlags 0x20000: sMimic, sLeech, sMCV, bCloakedFighter,
    // bIonArrayFrigate, p1IonArrayFrigate, p1StandardCorvette) keep a per-player detected flag in their specifics;
    // every other ship is always detected.
    if ((playerIndex < MAX_MULTIPLAYER_PLAYERS) && (ship->staticinfo->catTypeFlags & 0x20000))
    {
        // CAT: the cloaking types (bCloakedFighter specifics +0x30, bIonArrayFrigate/p1IonArrayFrigate +0x18;
        //      Cloak.h) [M2-R3 ships-a]
        bool32 *detected = cat_cloakDetectedFlags(ship);

        if (detected != NULL)
        {
            return detected[playerIndex];
        }
        // CAT-PROVISIONAL: the mimic/MCV/p1StandardCorvette (+0xAC) and leech (+0x1C) flags are ported with those
        //      ships in M2-R4 #4 (ship code B); until then they count as detected, as HW1 treated uncloaked ships.
        return TRUE;
    }
    return TRUE;
}

static void cat_speechAlertsNoteShip(Ship *ship, sdword playerIndex)    // 0x00522bc0
{
    // CAT-PROVISIONAL: the exe raises a "stealth ship spotted" speech-alert flag (0x00c09820 for the cloaking types,
    //      0x00c09824 mimics, 0x00c09814 leeches) when an enemy stealth ship of the local player's view is
    //      detected; the speech-alert runtime that reads them is stage 3, so nothing is raised here yet.
    (void)ship;
    (void)playerIndex;
}

void cat_shipStealthDetectedSet(Ship *ship, sdword playerIndex, bool32 detected, bool32 silent)    // 0x0053fad0
{
    // CAT: the cloaking types (bCloakedFighter, bIonArrayFrigate, p1IonArrayFrigate) [M2-R3 ships-a]: a ship that
    //      is fully cloaked and becomes detected by a player raises the speech alert unless silent
    bool32 *flags = cat_cloakDetectedFlags(ship);

    if (flags != NULL)
    {
        if ((flags[playerIndex] == FALSE) && (detected == TRUE) && (!silent) && cat_shipIsCloakedFully(ship))
        {
            cat_speechAlertsNoteShip(ship, playerIndex);
        }
        flags[playerIndex] = detected;
        return;
    }
    // CAT-PROVISIONAL: the mimic types (sMimic, sMCV, p1StandardCorvette: +0xAC, with the "appears as" rules) and the
    //      leech (+0x1C) are M2-R4 #4 (ship code B); until then their flags are not kept (see cat_shipDetectedByPlayer)
}

bool32 cat_shipIsMimicking(Ship *ship)                              // 0x005b99b0
{
    // CAT-PROVISIONAL: a ship with CAT_SHIPFLAG_Mimic mimics while its mimic mode (specifics, Ship+0x760) is not 0;
    //      the mimic code is ported in M2-R4 #4 (ship code B), and until then no mimic is active.
    (void)ship;
    return FALSE;
}

ShipType cat_mimicGetApparentShipType(Ship *ship)                   // 0x005b97e0
{
    // CAT-PROVISIONAL: a mimic shows the type in its specifics (Ship+0x704); see cat_shipIsMimicking
    return ship->shiptype;
}

real32 cat_mimicGetSelectRadius(Ship *ship)                         // 0x005b94e0
{
    // CAT-PROVISIONAL: an active mimic uses the apparent type's (or its stored, Ship+0x724) radius; see
    //      cat_shipIsMimicking. Otherwise the ship's own collision sphere.
    return ship->staticinfo->common.staticCollInfo.collspheresize;
}

bool32 cat_mimicAppearsAlliedTo(Ship *ship, struct Player *player)  // 0x005b96f0
{
    // CAT-PROVISIONAL: an active mimic posing as a ship of player looks allied to it (specifics Ship+0x71c); see
    //      cat_shipIsMimicking
    (void)player;
    return cat_shipIsMimicking(ship);
}

bool32 cat_mimicAppearsAsResource(Ship *ship, struct Player *player)    // 0x005b9860
{
    // CAT-PROVISIONAL: an undetected active mimic posing as an asteroid; see cat_shipIsMimicking
    (void)player;
    return cat_shipIsMimicking(ship);
}

bool32 cat_mimicAppearsAsDerelict(Ship *ship, struct Player *player)    // 0x005b98d0
{
    // CAT-PROVISIONAL: an undetected active mimic posing as a derelict; see cat_shipIsMimicking
    (void)player;
    return cat_shipIsMimicking(ship);
}

bool32 cat_spShipCanHyperspace(Ship *ship)                          // 0x00517650 (single player, SinglePlayer.c block)
{
    bool32 can = TRUE;

    if ((!allianceIsShipAlly(ship, universe.curPlayerPtr)) || bitTest(ship->header.flags, SOF_Dead) ||
        bitTest(ship->header.flags, SOF_Disabled) || (ship->shiptype == sSwarmer) || (ship->shiptype == bSwarmer))
    {
        can = FALSE;
    }
    if (ship->catDisabled && (ship->staticinfo->shipclass != CLASS_Component))
    {
        if (ship->shiptype != dEcclesiam)
        {
            can = FALSE;
        }
        // the exe lets a disabled Kushan carrier jump in mission 5 (singlePlayerGameInfo.currentMission == 5)
        if ((singlePlayerGameInfo.currentMission == 5) && (ship->shiptype == kCarrier))
        {
            can = TRUE;
        }
        if (ship->shiptype == sSentinel)
        {
            return TRUE;
        }
    }
    return can;
}
