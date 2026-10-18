// =============================================================================
//  CommandWrap.c
//  - Wrapper functions for calling/sending CommandLayer commands across network
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 7/30/1997 by gshaw
// =============================================================================

#include "CommandWrap.h"

#include "CommandDefs.h"
#include "CommandNetwork.h"
#include "InfoOverlay.h"
#include "MultiplayerGame.h"
#include "Options.h"
#include "Select.h"
#include "ShipSelect.h"
#include "Timer.h"
#include "Ships.h"
#include "SoundEvent.h"
#include "SpeechEvent.h"
#include "Tutor.h"
#include "Tweak.h"
#include "Universe.h"


/*-----------------------------------------------------------------------------
    Name        : clCommandMessage                                  0x004ac820
    Description : Puts a command message into the info overlay's message scroller
    Inputs      : CommandMessage - the text, colour - its colour (the exe's second argument)
    Outputs     : the message list of InfoOverlay.c
    Return      : void
----------------------------------------------------------------------------*/
// CAT (M2-R5 hud): Cataclysm's clCommandMessage(msg, flags) is the message scroller's (a 25-line list of 0x224-byte
//      entries that scrolls with cat_ioMessageScroll, the new-message sound 0x12a unless the text is strSavedGame):
//      the body is cat_ioCommandMessage; HW1's gMessage slots and mrCommandMessageDraw are retired
void clCommandMessage(char *CommandMessage, udword colour)
{
    cat_ioCommandMessage(CommandMessage, colour);
}

/*-----------------------------------------------------------------------------
    Name        : clFlashShips
    Description : Sets all ships in selection to flash
    Inputs      : selection - the selection to flash
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void clFlashShips(SelectCommand *selection)
{
    udword i;
    sdword j;

    for (i = 0; i < selection->numShips; i++)
    {
        Ship *ship = selection->ShipPtr[i];
        // CAT: clFlashShips 0x00452960: the flash time is real time (cat_taskRealTimeElapsed), and the objects clamped to
        //      a ship (its clampInfo's clamped-object list, +0x34) flash with it
        ship->targetable.flashtimer = cat_taskRealTimeElapsed;
        if ((ship->header.objtype == OBJ_ShipType) && (ship->guidance.clampInfo != NULL) &&
            (ship->guidance.clampInfo->clampedObjs != NULL))
        {
            SelectCommand *clamped = (SelectCommand *)ship->guidance.clampInfo->clampedObjs;
            for (j = 0; j < clamped->numShips; j++)
            {
                clamped->ShipPtr[j]->targetable.flashtimer = cat_taskRealTimeElapsed;
            }
        }
    }
}

// CAT: SpeechEventsForFuel is gone: Cataclysm has no fuel, and clWrapMove 0x004529e0 keeps only an empty scan of the
//      selection in its place (no effect)

// CAT: clWrapMove 0x004529e0 takes the waypoint path of clMove (pathType, from, numPoints, points)
void clWrapMove(CommandLayer *comlayer,SelectCommand *selectcom,sdword pathType,vector from,sdword numPoints,vector *points)
{
    if (playPackets || (universePause && !opPauseOrders) ) return;

//   0  1
// 0 0  0
// 1 1  0

    makeShipsControllable(selectcom,COMMAND_MOVE);
    if (selectcom->numShips == 0) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);

#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("Move Command Received", 0xffffffff);
#endif
    }

    // ships selected to move flash for a few frames to indicate the selection
    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        // CAT: no fuel speech and no tutorial message (clWrapMove 0x004529e0)
        clMove(comlayer,selectcom,pathType,from,numPoints,points);
    }
    else
    {
        // CAT-TODO(stage 5): the exe's clSendMove 0x0044eb20 sends the whole path (its packet is sized by numPoints);
        //      this tree's packet is HW1's from/to, so a network or recorded move goes to the last point only
        clSendMove(comlayer,selectcom,from,points[numPoints - 1]);
    }
}

void clWrapMpHyperspace(CommandLayer *comlayer,SelectCommand *selectcom,vector from,vector to)
{
    udword i;

    // CAT: paused orders are allowed with opPauseOrders, as for the other wraps (clWrapMpHyperspace 0x00452ae0)
    if (playPackets || (universePause && !opPauseOrders) ) return;

    //makeShipsControllable(selectcom,COMMAND_MP_HYPERSPACING);
    if (selectcom->numShips == 0) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);

#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("Hyperspace Command Received", 0xffffffff);
#endif
    }

    // make the ships unselectable

    // ships selected to move flash for a few frames to indicate the selection
    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clMpHyperspace(comlayer,selectcom,from,to);
    }
    else
    {
        clSendMpHyperspace(comlayer,selectcom,from,to);
    }
    for(i=0;i<selectcom->numShips;)
    {
        // CAT: the ship is disabled for reason 6 (hyperspacing) instead of losing SOF_Selectable (clWrapMpHyperspace 0x00452ae0)
        cat_shipSetDisableReason(selectcom->ShipPtr[i],TRUE,6);
        if(clRemoveShipFromSelection(&selSelected, selectcom->ShipPtr[i]))
        {
            ioUpdateShipTotals();
            continue;
        }
        i++;
    }
}


void clWrapAttack(CommandLayer *comlayer,SelectCommand *selectcom,AttackCommand *attackcom)
{
    if (playPackets || (universePause && !opPauseOrders) ) return;

    makeShipsControllable(selectcom,COMMAND_ATTACK);
    if (selectcom->numShips == 0) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips((SelectCommand *)attackcom);

#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("Attack Command Received", 0xffffffff);
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        if((tutorial==TUTORIAL_ONLY) && !tutEnable.bAttack)
            return;

        clAttack(comlayer,selectcom,attackcom);
    }
    else
    {
        clSendAttack(comlayer,selectcom,attackcom);
    }
}

// CAT: clWrapFormation 0x00452ca0 takes and passes on the force-field level (clFormation's fourth argument)
void clWrapFormation(CommandLayer *comlayer,SelectCommand *selectcom,TypeOfFormation formation,sdword catForceFieldLevel)
{
    if (playPackets || (universePause && !opPauseOrders) ) return;

    makeShipsControllable(selectcom,COMMAND_NULL);
    if (selectcom->numShips == 0) return;

    // CAT: no formation check here (clWrapFormation 0x00452ca0; clFormation checks)

    if ((!universe.aiplayerProcessing) && (!((tutorial==TUTORIAL_ONLY) && !tutEnable.bFormation)))
    {
        clFlashShips(selectcom);

#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("Formation Command Received", 0xffffffff);
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        if((tutorial==TUTORIAL_ONLY) && !tutEnable.bFormation)
            return;

        clFormation(comlayer,selectcom,formation,catForceFieldLevel);
    }
    else
    {
        // CAT-TODO(stage 5): the exe's formation packet carries the force-field level (clSendFormation 0x0044ef10)
        clSendFormation(comlayer,selectcom,formation);
    }
}

void clWrapDock(CommandLayer *comlayer,SelectCommand *selectcom,DockType dockType,ShipPtr dockwith)
{
    if (playPackets || (universePause && !opPauseOrders) ) return;

    //Although research ships have docking capabilities, they aren't allowed to be
    //ordered to dock:
    makeShipsControllable(selectcom,COMMAND_DOCK);
    if (selectcom->numShips == 0) return;

    // CAT: clWrapDock 0x00452d60: no research ships to drop (MakeSelectionNonResearchShips is gone); for any purpose
    //      but retire (7) it refreshes the ship totals instead (M2-R5 dock: the purposes)
    if (dockType != CAT_DOCK_RETIRE)
    {
         ioUpdateShipTotals();
    }

    if (selectcom->numShips == 0)
    {
        return;
    }

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);

#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("Dock Command Received", 0xffffffff);
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        if((tutorial==TUTORIAL_ONLY) && !tutEnable.bDock)
            return;

        clDock(comlayer,selectcom,dockType,dockwith);
    }
    else
    {
        clSendDock(comlayer,selectcom,dockType,dockwith);
    }

    if (!universe.aiplayerProcessing)
    {
        Ship *ship = selectcom->ShipPtr[0];

        if (ship->dockvars.dockship != NULL)
        {
            // CAT: the exe's speech by purpose (0x00452ed6): the fleet's retire line for 7, the ship's slipgate line for 10,
            //      its dock line otherwise, all through speechEventQueue with the exe's ids 0x400106a, 0x4000801 and
            //      0x4000803 (Cataclysm's speech ids: SpeechEvent.h, M2-R5 sound)
            if (dockType == CAT_DOCK_RETIRE)
            {
                speechEventQueue(NULL, 0x400106a, -1, -1, 0, ship->playerowner->playerIndex, -1, 5.0f, -1);
            }
            else if (dockType == CAT_DOCK_SLIPGATE)
            {
                speechEventQueue(ship, 0x4000801, ship->shiptype, -1, -1, -1, -1, 0.0f, -1);
            }
            else
            {
                speechEventQueue(ship, 0x4000803, ship->shiptype, -1, -1, -1, -1, 0.0f, -1);
            }
        }
    }
}

void clWrapDeterministicBuild(udword command,
                              CommandLayer* comlayer, sdword numShips,
                              ShipType shipType, ShipRace shipRace,
                              uword playerIndex, ShipPtr creator)
{
    if (playPackets) return;

    if (!universe.aiplayerProcessing)
    {
#if CL_TEXTFEEDBACK
        //@todo verbosify this lame message
        if (enableTextFeedback) clCommandMessage("New Build Command", 0xffffffff);
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clDeterministicBuild(command, comlayer, numShips, shipType, shipRace, playerIndex, creator);
    }
    else
    {
        clSendDeterministicBuild(command, comlayer, numShips, shipType, shipRace, playerIndex, creator);
    }
}

Ship *clWrapCreateShip(CommandLayer *comlayer,ShipType shipType,ShipRace shipRace,uword playerIndex,ShipPtr creator)
{
    if (playPackets) return NULL;   // CAT: the exe returns the flag's value here (0x00452f96); every caller returns first

    if (!universe.aiplayerProcessing)
    {
#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("New Ship Created", 0xffffffff);
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        return clCreateShip(comlayer,shipType,shipRace,playerIndex,creator);
    }
    else
    {
        clSendCreateShip(comlayer,shipType,shipRace,playerIndex,creator);
    }
    return NULL;
}

void clWrapBuildShip(CommandLayer *comlayer,ShipType shipType,ShipRace shipRace,uword playerIndex,ShipPtr creator)
{
    if (playPackets) return;

    if (!universe.aiplayerProcessing)
    {
#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("New Ship Being Built", 0xffffffff);
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clBuildShip(comlayer,shipType,shipRace,playerIndex,creator);
    }
    else
    {
        clSendBuildShip(comlayer,shipType,shipRace,playerIndex,creator);
    }

}

void clWrapCollectResource(CommandLayer *comlayer,SelectCommand *selectcom,ResourcePtr resource)
{
    if (playPackets || (universePause && !opPauseOrders) ) return;

    makeShipsControllable(selectcom,COMMAND_COLLECT_RESOURCES);
    if (selectcom->numShips == 0) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);

        if (resource != NULL)
        {
            resource->targetable.flashtimer = cat_taskRealTimeElapsed;   // CAT: real time (clWrapCollectResource 0x00453000)
        }
#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("Harvest Command Received", 0xffffffff);
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clCollectResource(comlayer,selectcom,resource);
    }
    else
    {
        clSendCollectResource(comlayer,selectcom,resource);
    }

}

void clWrapProtect(CommandLayer *comlayer,SelectCommand *selectcom,ProtectCommand *protectcom)
{
    if (playPackets || (universePause && !opPauseOrders) ) return;

    makeShipsControllable(selectcom,COMMAND_NULL);
    if (selectcom->numShips == 0) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips((SelectCommand *)protectcom);

#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("Protect Command Received", 0xffffffff);
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clProtect(comlayer,selectcom,protectcom);
    }
    else
    {
        clSendProtect(comlayer,selectcom,protectcom);
    }

}

void clWrapSpecial(CommandLayer *comlayer,SelectCommand *selectcom,SpecialCommand *targets)
{
    SelectCommand *copycom=NULL;
    sdword sizeofcopycom;
    if (playPackets || (universePause && !opPauseOrders) ) return;

    makeShipsControllable(selectcom,COMMAND_SPECIAL);
    if (selectcom->numShips == 0) return;

    sizeofcopycom = sizeofSelectCommand(selectcom->numShips);
    copycom = memAlloc(sizeofcopycom,"clspecialfix", Pyrophoric);
    memcpy(copycom,selectcom,sizeofcopycom);

    if (!universe.aiplayerProcessing)
    {
        //if the special operation has a target, flash the targets,
        //else flash the ships performing the operation
        if (targets)
        {
            clFlashShips((SelectCommand *)targets);
        }
        else
        {
            clFlashShips(selectcom);
        }

#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("Special Ability Activated", 0xffffffff);
#endif
    }

    // CAT: clWrapSpecial 0x004531d0: HW1's salvage-corvette block (the "not enough corvettes" and salvage speech, the
    //      salvage corvettes ordered one by one) is gone. The exe only runs cat_shipIsSalvager over the selection of
    //      the current player's targeted order and ignores the result, so nothing of it is kept here. The copy is
    //      always freed (HW1 leaked it on the tutorial return).
    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        if (!((tutorial==TUTORIAL_ONLY) && !tutEnable.bSpecial) && (copycom->numShips > 0))
        {
            clSpecial(comlayer,copycom,targets);
        }
    }
    else
    {
        if (copycom->numShips > 0)
        {
            clSendSpecial(comlayer,copycom,targets);
        }
    }
    memFree(copycom);
}

void clWrapHalt(CommandLayer *comlayer,SelectCommand *selectcom)
{
    if (playPackets || (universePause && !opPauseOrders) ) return;

    makeShipsControllable(selectcom,COMMAND_HALT);
    if (selectcom->numShips == 0) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);

#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("Halt Command Received", 0xffffffff);
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clHalt(comlayer,selectcom);
    }
    else
    {
        clSendMisc(comlayer,selectcom,MISCCOMMAND_HALT,0);
    }
}

void clWrapScuttle(CommandLayer *comlayer,SelectCommand *selectcom)
{
    // CAT: paused orders are allowed with opPauseOrders (clWrapScuttle 0x00453390)
    if (playPackets || (universePause && !opPauseOrders) ) return;

    //makeShipsControllable(selectcom);     // always allow scuttling (in case ship gets stuck in dock bay or something)
    if (selectcom->numShips == 0) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);

#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("Scuttle Command Received", 0xffffffff);
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clScuttle(comlayer,selectcom);
    }
    else
    {
        clSendMisc(comlayer,selectcom,MISCCOMMAND_SCUTTLE,0);
    }
}

void clWrapAutoLaunch(udword OnOff,udword playerIndex)
{
    if (playPackets) return;

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clAutoLaunch(OnOff,playerIndex);
    }
    else
    {
        clSendAutoLaunch(OnOff,playerIndex);
    }
}

void clWrapSetAlliance(udword AllianceType, uword playerone, uword playertwo)
{
    if (playPackets) return;

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clSetAlliance(AllianceType, playerone, playertwo);
    }
    else
    {
        clSendSetAlliance(AllianceType, playerone, playertwo);
    }

}

void clWrapLaunchMultipleShips(CommandLayer *comlayer,SelectCommand *selectcom,ShipPtr launchFrom)
{
    if (playPackets) return;

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clLaunchMultipleShips(comlayer,selectcom,launchFrom);
    }
    else
    {
        clSendLaunchMultipleShips(comlayer,selectcom,launchFrom);
    }
}

void clWrapSetTactics(CommandLayer *comlayer,SelectCommand *selectcom,TacticsType tacticstype)
{
    if (playPackets || (universePause && !opPauseOrders) ) return;
    if (selectcom->numShips == 0) return;     // CAT: an empty selection does nothing (clWrapSetTactics 0x00453500)

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);

#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("Tactics Command Received", 0xffffffff);
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clSetTactics(comlayer,selectcom,tacticstype);
    }
    else
    {
        clSendMisc(comlayer,selectcom,MISCCOMMAND_TACTICS,tacticstype);
    }
}

void clWrapRUTransfer(CommandLayer *comlayer,sdword fromIndex,sdword toIndex, sdword resourceUnits,ubyte flags)
{
    if (playPackets) return;

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        //needed for ResourceDeductions that are deterministic (building crud)
        clRUTransfer(comlayer,toIndex,fromIndex,resourceUnits,flags);
    }
    else
    {
        clSendRuTransfer(comlayer,fromIndex,toIndex,resourceUnits,flags);
    }

}

void clWrapSetKamikaze(CommandLayer *comlayer,SelectCommand *selectcom)
{
    // CAT: paused orders are allowed with opPauseOrders (clWrapSetKamikaze 0x00453820)
    if (playPackets || (universePause && !opPauseOrders) ) return;

    makeShipsControllable(selectcom,COMMAND_NULL);
    if (selectcom->numShips == 0) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);

#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("Kamikaze Command Received", 0xffffffff);
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clSetKamikaze(comlayer,selectcom);
    }
    else
    {
        clSendMisc(comlayer,selectcom,MISCCOMMAND_KAMIKAZE,0);  //0 = dummy num
    }
}

void clWrapSetMilitaryParade(CommandLayer *comlayer,SelectCommand *selectcom)
{
    if (playPackets || (universePause && !opPauseOrders) ) return;

    makeShipsControllable(selectcom,COMMAND_NULL);
    if (selectcom->numShips == 0) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);

#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("Parade Command Received", 0xffffffff);
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clSetMilitaryParade(comlayer,selectcom);
    }
    else
    {
        clSendMisc(comlayer,selectcom,MISCCOMMAND_PARADE,0);  //0 = dummy num
    }
}

void clWrapResearch(udword type, udword playernum, udword labnum, udword tech)
{
    if (playPackets) return;

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clSetResearch(type, playernum, labnum, tech);
    }
    else
    {
        clSendResearch(type, playernum, labnum, tech);
    }
}

/*-----------------------------------------------------------------------------
    CAT: the external-build wraps (cat_clWrapStartExternalBuildTimer 0x00453660, cat_clWrapExternalBuildUpdate
    0x00453a00). Unlike the order wraps they ignore universePause. CAT-TODO(stage 5): in a network or recorded
    game the exe sends clSendMisc(comlayer, selectcom, 11, 0) / cat_clSendExternalBuildUpdatePacket 0x004523a0
    instead; those packets are not ported, so such a game does nothing here yet.
----------------------------------------------------------------------------*/
void cat_clWrapStartExternalBuildTimer(CommandLayer *comlayer,SelectCommand *selectcom)
{
    if (playPackets || (selectcom->numShips == 0)) return;

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        cat_clStartExternalBuildTimer(comlayer,selectcom);
    }
}

void cat_clWrapExternalBuildUpdate(CommandLayer *comlayer,uword playerIndex,Ship *creator,Ship *ship,real32 progress,sdword mode)
{
    if (playPackets) return;

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        cat_clExternalBuildUpdate(comlayer,playerIndex,creator,ship,progress,mode);
    }
}

/*-----------------------------------------------------------------------------
    CAT: the wraps of Cataclysm's orders (link, unlink, Leech, module slots, Beast system, point, Mimic, upgrade,
    special target). Like the HW1 wraps they flash the ships (when no AI is ordering) and order directly in a
    single-player game; a network or recorded game sends the order instead: clSendMisc with the exe's misc command
    numbers 5-10 (the receiving side of those numbers is stage 5's: CAT-TODO(stage 5)), and for the point, Mimic,
    upgrade and special-target orders the exe's own packets (cat_clSendPointOrderPacket 0x00452120,
    cat_clSendMimicPacket 0x00452200, cat_clSendUpgradeShipsPacket 0x004522e0, cat_clSendSpecialTarget 0x00451fc0,
    cat_clSendSpecialTargetFlagged 0x00452040), which are not ported (CAT-TODO(stage 5)): such a game does nothing
    there yet. Only the Leech and special-target wraps test universePause (with opPauseOrders).
----------------------------------------------------------------------------*/
#define CAT_MISCCOMMAND_LINK            5
#define CAT_MISCCOMMAND_UNLINK          6
#define CAT_MISCCOMMAND_LEECHACTION     7
#define CAT_MISCCOMMAND_UNLEECH         8
#define CAT_MISCCOMMAND_MODULESLOT      9
#define CAT_MISCCOMMAND_BEASTSYSTEM     10

// CAT: new (cat_clWrapLink 0x00453940)
void cat_clWrapLink(CommandLayer *comlayer,SelectCommand *selectcom)
{
    if (playPackets || (selectcom->numShips == 0)) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        cat_clLink(comlayer,selectcom);
    }
    else
    {
        clSendMisc(comlayer,selectcom,CAT_MISCCOMMAND_LINK,0);
    }
}

// CAT: new (cat_clWrapUnlink 0x004539a0)
void cat_clWrapUnlink(CommandLayer *comlayer,SelectCommand *selectcom)
{
    if (playPackets || (selectcom->numShips == 0)) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        cat_clUnlink(comlayer,selectcom);
    }
    else
    {
        clSendMisc(comlayer,selectcom,CAT_MISCCOMMAND_UNLINK,0);
    }
}

// CAT: new (cat_clWrapSetLeechAction 0x00453730)
void cat_clWrapSetLeechAction(CommandLayer *comlayer,SelectCommand *selectcom,ubyte mode)
{
    if (playPackets || (universePause && !opPauseOrders) || (selectcom->numShips == 0)) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        cat_clSetLeechAction(comlayer,selectcom,mode);
    }
    else
    {
        clSendMisc(comlayer,selectcom,CAT_MISCCOMMAND_LEECHACTION,(udword)mode);
    }
}

// CAT: new (cat_clWrapUnleech 0x004536b0)
void cat_clWrapUnleech(CommandLayer *comlayer,SelectCommand *selectcom)
{
    if (playPackets || (universePause && !opPauseOrders) || (selectcom->numShips == 0)) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        cat_clUnleech(comlayer,selectcom);
    }
    else
    {
        clSendMisc(comlayer,selectcom,CAT_MISCCOMMAND_UNLEECH,0);
    }
}

// CAT: new (cat_clWrapSetModuleSlot 0x00453580): no flash
void cat_clWrapSetModuleSlot(CommandLayer *comlayer,SelectCommand *selectcom,sdword moduleType,sdword count)
{
    if (playPackets || (selectcom->numShips == 0)) return;

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        cat_clSetModuleSlot(comlayer,selectcom,moduleType,count);
    }
    else
    {
        clSendMisc(comlayer,selectcom,CAT_MISCCOMMAND_MODULESLOT,
                   cat_clModuleTypeToMiscBits(moduleType) | cat_clModuleSlotToMiscBits(count));
    }
}

// CAT: new (cat_clWrapSetBeastSystem 0x004535f0): no flash
void cat_clWrapSetBeastSystem(CommandLayer *comlayer,Ship *ship,sdword system)
{
    SelectCommand selectone;

    if (playPackets) return;

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        cat_clSetBeastSystem(comlayer,ship,system);
    }
    else
    {
        selectone.numShips = 1;
        selectone.timeLastStatus = 0.0f;     // CAT-FIX: the exe leaves it uninitialised (0x004535f0)
        selectone.ShipPtr[0] = ship;
        clSendMisc(comlayer,&selectone,CAT_MISCCOMMAND_BEASTSYSTEM,(udword)system);
    }
}

// CAT: new (cat_clWrapPointOrder 0x00453a70)
void cat_clWrapPointOrder(CommandLayer *comlayer,SelectCommand *selectcom,vector *point)
{
    if (playPackets) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        cat_clPointOrder(comlayer,selectcom,point);
    }
    // CAT-TODO(stage 5): else cat_clSendPointOrderPacket 0x00452120
}

// CAT: new (cat_clWrapMimic 0x00453b30)
void cat_clWrapMimic(CommandLayer *comlayer,SelectCommand *selectcom,udword mode,udword shipType,udword player,udword index)
{
    if (playPackets) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        cat_clMimic(comlayer,selectcom,mode,shipType,player,index);
    }
    // CAT-TODO(stage 5): else cat_clSendMimicPacket 0x00452200
}

// CAT: new (cat_clWrapUpgradeShips 0x00453ad0)
void cat_clWrapUpgradeShips(CommandLayer *comlayer,SelectCommand *selectcom,bool32 cancel)
{
    if (playPackets) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips(selectcom);
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        cat_clUpgradeShips(comlayer,selectcom,cancel);
    }
    // CAT-TODO(stage 5): else cat_clSendUpgradeShipsPacket 0x004522e0
}

// CAT: new (cat_clWrapSpecialTarget 0x00453130): the targets flash
void cat_clWrapSpecialTarget(CommandLayer *comlayer,SelectCommand *selectcom,SpecialCommand *targets,bool32 flag)
{
    if (playPackets || (universePause && !opPauseOrders) || (selectcom->numShips == 0)) return;

    if (!universe.aiplayerProcessing)
    {
        clFlashShips((SelectCommand *)targets);
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        cat_clSpecialTarget(comlayer,selectcom,targets,flag);
    }
    // CAT-TODO(stage 5): else cat_clSendSpecialTargetFlagged 0x00452040 (flag set) / cat_clSendSpecialTarget 0x00451fc0
}
