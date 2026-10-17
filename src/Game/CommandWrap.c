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
    Name        : clCommandMessage
    Description : Inserts a command message into the global gMessage variable
                  to be displayed on screen later
    Inputs      : String to be inserted into gMessage
    Outputs     : Modifies global gMessage variable
    Return      : void
----------------------------------------------------------------------------*/
// CAT-TODO(stage 3): clCommandMessage 0x004ac820 lives in Cataclysm's message scroller (InfoOverlay area): two
//      arguments (msg, flags), a 25-line ring of 0x224-byte entries that scrolls with cat_ioMessageScroll, a sound
//      (soundEventPlay); HW1's gMessage slots stay until the UI port
void clCommandMessage(char CommandMessage[MAX_MESSAGE_LENGTH])
{
    ubyte i = 0;

    LockMutex(gMessageMutex);

    // find the next empty message slot in the global message array
    while ((i < MAX_MESSAGES)&&(gMessage[i].message[0] != '\0'))
    {
        i++;
    }

    gMessage[i].MessageExpire = universe.totaltimeelapsed + TW_MESSAGE_DELAY_TIME;
    strcpy(gMessage[i].message, CommandMessage);

    UnLockMutex(gMessageMutex);
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

void clWrapMove(CommandLayer *comlayer,SelectCommand *selectcom,vector from,vector to)
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
        if (enableTextFeedback) clCommandMessage("Move Command Received");
#endif
    }

    // ships selected to move flash for a few frames to indicate the selection
    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        // CAT: no fuel speech and no tutorial message (clWrapMove 0x004529e0)
        clMove(comlayer,selectcom,from,to);
    }
    else
    {
        clSendMove(comlayer,selectcom,from,to);
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
        if (enableTextFeedback) clCommandMessage("Hyperspace Command Received");
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
        if (enableTextFeedback) clCommandMessage("Attack Command Received");
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

void clWrapFormation(CommandLayer *comlayer,SelectCommand *selectcom,TypeOfFormation formation)
{
    if (playPackets || (universePause && !opPauseOrders) ) return;

    makeShipsControllable(selectcom,COMMAND_NULL);
    if (selectcom->numShips == 0) return;

    // CAT: no formation check here (clWrapFormation 0x00452ca0; clFormation checks). CAT-TODO: the exe's wrap takes and
    //      passes on clFormation's fourth argument (the force-field level), see clFormation

    if ((!universe.aiplayerProcessing) && (!((tutorial==TUTORIAL_ONLY) && !tutEnable.bFormation)))
    {
        clFlashShips(selectcom);

#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("Formation Command Received");
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        if((tutorial==TUTORIAL_ONLY) && !tutEnable.bFormation)
            return;

        clFormation(comlayer,selectcom,formation);
    }
    else
    {
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
        if (enableTextFeedback) clCommandMessage("Dock Command Received");
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
            //      its dock line otherwise, all through speechEventQueue. CAT-TODO(M2-R5 sound): the event ids 0x400106a,
            //      0x4000801 and 0x4000803 are passed as the exe does; the speech table that resolves them is stage 3's
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
        if (enableTextFeedback) clCommandMessage("New Build Command");
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

void clWrapCreateShip(CommandLayer *comlayer,ShipType shipType,ShipRace shipRace,uword playerIndex,ShipPtr creator)
{
    if (playPackets) return;

    if (!universe.aiplayerProcessing)
    {
#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("New Ship Created");
#endif
    }

    if ((!multiPlayerGame) && (!recordFakeSendPackets))
    {
        clCreateShip(comlayer,shipType,shipRace,playerIndex,creator);
    }
    else
    {
        clSendCreateShip(comlayer,shipType,shipRace,playerIndex,creator);
    }

}

void clWrapBuildShip(CommandLayer *comlayer,ShipType shipType,ShipRace shipRace,uword playerIndex,ShipPtr creator)
{
    if (playPackets) return;

    if (!universe.aiplayerProcessing)
    {
#if CL_TEXTFEEDBACK
        if (enableTextFeedback) clCommandMessage("New Ship Being Built");
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
        if (enableTextFeedback) clCommandMessage("Harvest Command Received");
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
        if (enableTextFeedback) clCommandMessage("Protect Command Received");
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
        if (enableTextFeedback) clCommandMessage("Special Ability Activated");
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
        if (enableTextFeedback) clCommandMessage("Halt Command Received");
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
        if (enableTextFeedback) clCommandMessage("Scuttle Command Received");
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
        if (enableTextFeedback) clCommandMessage("Tactics Command Received");
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
        if (enableTextFeedback) clCommandMessage("Kamikaze Command Received");
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
        if (enableTextFeedback) clCommandMessage("Parade Command Received");
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
