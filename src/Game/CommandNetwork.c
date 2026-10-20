// =============================================================================
//  CommandNetwork.c
//  - Handles all of the packing/unpacking, sending/receiving of command
//    packets and sync packets
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 7/30/1997 by gshaw
// =============================================================================

#include "CommandNetwork.h"

#include "AutoDownloadMap.h"
#include "HorseRace.h"
#include "LagPrint.h"
#include "MultiplayerGame.h"
#include "NetCheck.h"
#include "OracleDump.h"     // HWI-ORACLE (M2-R8 oracle-rung-physics): the per-packet half of /oracleShipDump
#include "Sensors.h"
#include "TimeoutTimer.h"
#include "Titan.h"
#include "TitanNet.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "utility.h"

/*=============================================================================
    Private Defines:
=============================================================================*/

#define HIGH_WATER_MARK     10

/*=============================================================================
    Private Variables:
=============================================================================*/

udword syncPacketBeforeGameStarted = 0;

udword packetBeforeGameStarted = 0;
udword unknownPackets = 0;
udword printedUnknownPackets = 0;

udword sentPacketNumber = 0;
udword receivedPacketNumber = 0;

udword syncpkts = 0;
udword cmdpkts = 0;
udword syncoverruns = 0;
udword cmdoverruns = 0;

// Keep Alive variables

#define ALIVESTATUS_ALIVE                   0
#define ALIVESTATUS_QUERYINGDROPOUT         1
#define ALIVESTATUS_WAITQUERYDROPOUTLATER   2
#define ALIVESTATUS_DROPPEDOUT              3

#define KILLDROPPEDOUTPLAYER_VERIFY     0xfe389bca

TTimer SendIAmAliveTimer;
TTimer SendAliveStatusTimer;

void *AliveTimeoutStatusesMutex = NULL;
TTimer AliveTimeoutTimers[MAX_MULTIPLAYER_PLAYERS];
ubyte AliveStatuses[MAX_MULTIPLAYER_PLAYERS];
bool8 HaveKilledPlayerDueToDropout[MAX_MULTIPLAYER_PLAYERS];

bool32 KeepAliveCalledFirstTime = FALSE;

// variables for printing out the message when a player has dropped out
udword numPlayerDropped = 0;
udword playersDropped[MAX_MULTIPLAYER_PLAYERS];
real32 printTimeout = 0.0f;

/*=============================================================================
    Private functions:
=============================================================================*/

void KeepAliveStartTimers(void);
void receivedIAmAlivePacket(HWPacketHeader *packet,udword sizeofPacket);
void receivedAliveStatusPacket(AliveStatusPacket *packet,udword sizeofPacket);

void BroadCastHorseRacePacketFromCaptain(ubyte *packet, udword sizeofpacket);
void BroadCastChatPacketFromCapatain(ChatPacket *packet, udword sizeofpacket);

void clPlayerDropped(udword playerMask,udword verify);
void clSendPlayerDropped(udword playerDroppedMask);

/*=============================================================================
    Functions:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : CommandNetworkReset
    Description : resets the CommandNetwork
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void CommandNetworkReset(void)
{
    sdword i;

    syncPacketBeforeGameStarted = 0;

    packetBeforeGameStarted = 0;
    unknownPackets = 0;
    printedUnknownPackets = 0;

    sentPacketNumber = 0;
    receivedPacketNumber = 0;

    syncpkts = 0;
    cmdpkts = 0;
    syncoverruns = 0;
    cmdoverruns = 0;

    printCaptainMessage = FALSE;
    numPlayerDropped = 0;
    for (i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
    {
        playersDropped[i] = 0;
    }
    printTimeout = 0;

    utyPlayerDroppedDisplay = -1;
}

/*-----------------------------------------------------------------------------
    Name        : ReceivedSyncPacketCB
    Description : callback function which is called whenever a sync packet is
                  received.
    Inputs      : packet, sizeofPacket
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ReceivedSyncPacketCB(ubyte *packet,udword sizeofPacket)
{
    dbgAssertOrIgnore(((HWPacketHeader *)packet)->type == PACKETTYPE_SYNC);

    LockQueue(&ProcessSyncPktQ);
    HWEnqueue(&ProcessSyncPktQ,packet,sizeofPacket);
    UnLockQueue(&ProcessSyncPktQ);
}

/*-----------------------------------------------------------------------------
    Name        : ReceivedCmdPacketCB
    Description : callback function which is called whenever a command packet is
                  received.
    Inputs      : packet, sizeofPacket
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ReceivedCmdPacketCB(ubyte *packet,udword sizeofPacket)
{
    dbgAssertOrIgnore(((HWPacketHeader *)packet)->type == PACKETTYPE_COMMAND);

    if (IAmCaptain)
    {
        LockQueue(&ProcessCmdPktQ);
        HWEnqueue(&ProcessCmdPktQ,packet,sizeofPacket);
        UnLockQueue(&ProcessCmdPktQ);
    }
    // else throw packet away
}

/*-----------------------------------------------------------------------------
    Name        : ReceivedSyncPacketCB
    Description : callback function which is called whenever a sync packet is
                  received.
    Inputs      : packet, sizeofPacket
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ReceivedRequestedSyncPacketCB(ubyte *packet,udword sizeofPacket)
{
    dbgAssertOrIgnore(((HWPacketHeader *)packet)->type == PACKETTYPE_REQUESTEDSYNC);

    LockQueue(&ProcessRequestedSyncPktQ);
    HWEnqueue(&ProcessRequestedSyncPktQ,packet,sizeofPacket);
    UnLockQueue(&ProcessRequestedSyncPktQ);
}

void ReceivedRequestSyncPacketsPacketCB(ubyte *packet,udword sizeofPacket)
{
    udword frompacketnum;
    udword topacketnum;
    udword i;
    HWPacketHeader *sendpacket;
    udword sendpacketsize;
    bool32 gotit;
    uword sendto;

    dbgAssertOrIgnore(((HWPacketHeader *)packet)->type == PACKETTYPE_REQUESTSYNCPKTS);

    frompacketnum = ((RequestSyncPacketsPacket *)packet)->packetheader.frame;
    topacketnum = ((RequestSyncPacketsPacket *)packet)->topacketnum;
    sendto = ((RequestSyncPacketsPacket *)packet)->packetheader.from;

    dbgAssertOrIgnore(frompacketnum <= topacketnum);

    for (i=frompacketnum;i<=topacketnum;i++)
    {
        gotit = GetSyncPktFromLastSyncPktsQ(i,&sendpacket,&sendpacketsize);
        if (gotit)
        {
            sendpacket->from = sigsPlayerIndex;     // same sync pkt as before, but from me this time
            sendpacket->type = PACKETTYPE_REQUESTEDSYNC;    // and this also now a requested sync pkt
            titanSendPointMessage(sendto,(ubyte *)sendpacket,sendpacketsize);
        }
        else
        {
            if (i == topacketnum)   // always have to send the last packet requested for it to continue
            {
                HWPacketHeader fakelastsyncpkt;

                fakelastsyncpkt.type = PACKETTYPE_REQUESTEDSYNC;
                fakelastsyncpkt.from = sigsPlayerIndex;
                fakelastsyncpkt.frame = (udword)-1;
                fakelastsyncpkt.numberOfCommands = 0;

                titanSendPointMessage(sendto,(ubyte *)&fakelastsyncpkt,sizeof(HWPacketHeader));
            }
//            titanDebug("Warning: Do not have sync pkt %d to send by request to %d\n",i,sendto);
            dbgMessagef("Warning: Do not have sync pkt %d to send by request to %d\n",i,sendto);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : ReceivedTransferCaptaincyPacketCB
    Description : callback function which is called whenever a captaincy packet is received.
    Inputs      : packet, sizeofPacket
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ReceivedTransferCaptaincyPacketCB(ubyte *packet,udword sizeofPacket)
{
    dbgAssertOrIgnore(((HWPacketHeader *)packet)->type == PACKETTYPE_TRANSFERCAPTAINCY);

    LockQueue(&ProcessCaptaincyPktQ);
    HWEnqueue(&ProcessCaptaincyPktQ,packet,sizeofPacket);
    UnLockQueue(&ProcessCaptaincyPktQ);
}

bool32 checkPlayersReady(void)
{
    sdword i;
    bool32 ready = TRUE;

    dbgAssertOrIgnore((IAmCaptain) && (!multiPlayerGameUnderWay));

    for (i=0;i<sigsNumPlayers;i++)
    {
        if (i != captainIndex)
        {
            if (playersReadyToGo[i] == FALSE)
            {
                ready = FALSE;
            }
        }
    }

    return ready;
}

void PlayerDroppedOut(udword player,bool32 timedOut)
{
    if (player < sigsNumPlayers)
    {
        if (timedOut)
        {
            playersReadyToGo[player] = PLAYER_DROPPED_OUT;
            captaincyLog(FALSE,"Player %d dropped out",player);
        }
        else
        {
            playersReadyToGo[player] = PLAYER_QUIT;
        }

        if (player == captainIndex)
        {
            // captain just joined out, pick next captain in line:
            captainIndex = (captainIndex + 1) % sigsNumPlayers;
            receiveSyncPacketsFrom = captainIndex;
        }
    }
}

void PlayerInGameQuited(udword player)
{
    if (player < sigsNumPlayers)
    {
        playersReadyToGo[player] = PLAYER_QUIT;
    }
}

/*-----------------------------------------------------------------------------
    Name        : ReceivedPacketCB
    Description : callback function which is called whenever any packet is
                  received.  It dispatches the packet as appropriate.
    Inputs      : packet, sizeofPacket
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ReceivedPacketCB(ubyte *packet,udword sizeofPacket)
{
    if ((!gameIsRunning) && (!startingGame))
    {
        packetBeforeGameStarted++;

        if (((HWPacketHeader *)packet)->type == PACKETTYPE_SYNC)
        {
            syncPacketBeforeGameStarted++;
        }
        else if (((HWPacketHeader *)packet)->type == PACKETTYPE_CHAT)
        {
            if (IAmCaptain)
            {
                BroadCastChatPacketFromCapatain((ChatPacket *)packet, sizeofPacket);
            }
            else
                recievedChatPacketCB(packet, sizeofPacket);
        }
        else if (((HWPacketHeader *)packet)->type == PACKETTYPE_FILE)
        {
            receivedFilePacketCB(packet,sizeofPacket);
        }

        return;
    }

    switch (((HWPacketHeader *)packet)->type)
    {
        case PACKETTYPE_TRANSFERCAPTAINCY:
            ReceivedTransferCaptaincyPacketCB(packet,sizeofPacket);
            break;

        case PACKETTYPE_SYNC:
            ReceivedSyncPacketCB(packet,sizeofPacket);
            break;

        case PACKETTYPE_COMMAND:
            ReceivedCmdPacketCB(packet,sizeofPacket);
            break;

        case PACKETTYPE_REQUESTEDSYNC:
            ReceivedRequestedSyncPacketCB(packet,sizeofPacket);
            break;

        case PACKETTYPE_REQUESTSYNCPKTS:
            ReceivedRequestSyncPacketsPacketCB(packet,sizeofPacket);
            break;

        case PACKETTYPE_DROPPINGOUTOFLOAD:
            if (!multiPlayerGameUnderWay)
            {
                PlayerDroppedOut(((HWPacketHeader *)packet)->frame,FALSE);
            }
            break;

        case PACKETTYPE_INGAMEQUITING:
            PlayerInGameQuited(((HWPacketHeader *)packet)->frame);
            break;

        case PACKETTYPE_NONCAPTAINREADY:
            if (!multiPlayerGameUnderWay)
            {
                sdword player = ((HWPacketHeader *)packet)->frame;       // use frame field as player number

                if (player < sigsNumPlayers)
                {
                    playersReadyToGo[player] = TRUE;
                }
            }
            break;

        case PACKETTYPE_CHAT:       //assumption:   If we revieved the packet, we were meant to see it
            if (IAmCaptain)
            {
                BroadCastChatPacketFromCapatain((ChatPacket *)packet, sizeofPacket);
            }
            else
                recievedChatPacketCB(packet, sizeofPacket);

            break;
        case PACKETTYPE_HORSERACE:
            if(IAmCaptain)
            {
                BroadCastHorseRacePacketFromCaptain(packet,sizeofPacket);
            }
            recievedHorsePacketCB(packet,sizeofPacket);
            break;

        case PACKETTYPE_IAMALIVE:
            if ((!CaptainNotDefined) && (IAmCaptain))
            {
                receivedIAmAlivePacket((HWPacketHeader *)packet,sizeofPacket);
            }
            break;

        case PACKETTYPE_ALIVESTATUS:
            if ((!CaptainNotDefined) && (!IAmCaptain))
            {
                receivedAliveStatusPacket((AliveStatusPacket *)packet,sizeofPacket);
            }
            break;

        case PACKETTYPE_FILE:
            receivedFilePacketCB(packet,sizeofPacket);
            break;

        case PACKETTYPE_LAGCHECK:
            lagRecievedPacketCB(packet,sizeofPacket);
            break;

        case PACKETTYPE_CHEATDETECT:
            {
                if (!blobSyncErr)
                {
                    blobSyncErrFrame = ((HWPacketHeader *)packet)->frame;
                }
                blobSyncErr++;
            }
            break;

        default:
            unknownPackets++;
            break;
    }
}

/*-----------------------------------------------------------------------------
    Name        : SetTargetID
    Description : sets the TargetID based on the target
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SetTargetID(TargetID *targetID,SpaceObjRotImpTarg *target)
{
    targetID->objtype = (uword)target->header.objtype;
    switch (target->header.objtype)
    {
        case OBJ_ShipType:
            targetID->objNumber = ((Ship *)target)->shipID.shipNumber;
            break;

        case OBJ_MissileType:
            targetID->objNumber = ((Missile *)target)->missileID.missileNumber;
            break;

        case OBJ_AsteroidType:
        case OBJ_NebulaType:
        case OBJ_GasType:
        case OBJ_DustType:
            targetID->objNumber = ((Resource *)target)->resourceID.resourceNumber;
            break;

        case OBJ_DerelictType:
            targetID->objNumber = ((Derelict *)target)->derelictID.derelictNumber;
            break;
        default:
            break;
    }
}

/*-----------------------------------------------------------------------------
    Name        : convertNetSelectionToSelectCommand
    Description : converts a NetSelect type to a SelectCommand type.  It converts
                  all ShipID's to ShipPtr's.  Any dead ShipID's or ships that don't belong
                  to player from are rejected
    Inputs      :
    Outputs     :
    Return      : returns the allocated SelectCommand.  Caller must free it when done.
----------------------------------------------------------------------------*/
SelectCommand *convertNetSelectionToSelectCommand(NetSelection *netselection,bool32 considerInsideShips,uword from)
{
    udword numShips = netselection->numShips;
    udword selectCommandSize = sizeofSelectCommand(numShips);
    SelectCommand *selectCommand;
    bool32 someShipsDied = FALSE;
    bool32 someShipsOrderedByWrongPlayer = FALSE;
    udword i;
    uword shipPlayerIndex;

    dbgAssertOrIgnore(numShips > 0);

    selectCommand = memAlloc(selectCommandSize,"conv. SelCom",0);

    selectCommand->numShips = numShips;

    for (i=0;i<numShips;i++)
    {
        if ((selectCommand->ShipPtr[i] = ShipIDtoShip(netselection->ShipID[i],considerInsideShips)) == NULL)
        {
            someShipsDied = TRUE;
        }
        else
        {
            shipPlayerIndex = selectCommand->ShipPtr[i]->playerowner->playerIndex;
            
            // if it's a human ship
            if ((shipPlayerIndex >= 0) && (shipPlayerIndex < tpGameCreated.numPlayers))
            {
                // do further cheat check - only can control own ship
                if (shipPlayerIndex != from)
                {
                    selectCommand->ShipPtr[i] = NULL;       // reject ship
                    someShipsDied = TRUE;
                    someShipsOrderedByWrongPlayer = TRUE;
                }
            }
        }
    }

    if (someShipsDied)
    {
        if (someShipsOrderedByWrongPlayer)
            dbgMessage("WARNING: Some ships ordered by wrong player!");
        clRemoveShipFromSelection(selectCommand,NULL);  // remove NULL ship pointers from selectCommand
    }

    return selectCommand;
}

/*-----------------------------------------------------------------------------
    Name        : convertNetAttackSelectionToAttackCommand
    Description : converts a NetAttackSelection to a AttackCommand type.  It converts
                  all TargetID's to Target's.
    Inputs      :
    Outputs     :
    Return      : returns the allocated AttackCommand.  Caller must free it when done.
----------------------------------------------------------------------------*/
AttackCommand *convertNetAttackSelectionToAttackCommand(NetAttackSelection *netattackselection)
{
    udword numTargets = netattackselection->numTargets;
    udword attackCommandSize = sizeofAttackCommand(numTargets);
    AttackCommand *attackCommand;
    bool32 someTargetsDied = FALSE;
    udword i;
    TargetID targetID;
    ShipID shipID;
    MissileID missileID;
    ResourceID resourceID;
    DerelictID derelictID;

    dbgAssertOrIgnore(numTargets > 0);

    attackCommand = memAlloc(attackCommandSize,"conv. AttCom",0);

    attackCommand->numTargets = numTargets;

    for (i=0;i<numTargets;i++)
    {
        targetID = netattackselection->TargetID[i];

        switch (targetID.objtype)
        {
            case OBJ_ShipType:
                shipID.shipNumber = targetID.objNumber;
                attackCommand->TargetPtr[i] = (SpaceObjRotImpTarg *)ShipIDtoShip(shipID,FALSE);
                break;

            case OBJ_MissileType:
                missileID.missileNumber = targetID.objNumber;
                attackCommand->TargetPtr[i] = (SpaceObjRotImpTarg *)MissileIDtoMissilePtr(missileID);
                break;

            case OBJ_AsteroidType:
            case OBJ_NebulaType:
            case OBJ_GasType:
            case OBJ_DustType:
                resourceID.resourceNumber = targetID.objNumber;
                attackCommand->TargetPtr[i] = (SpaceObjRotImpTarg *)ResourceIDtoResourcePtr(resourceID);
                break;

            case OBJ_DerelictType:
                derelictID.derelictNumber = targetID.objNumber;
                attackCommand->TargetPtr[i] = (SpaceObjRotImpTarg *)DerelictIDToDerelictPtr(derelictID);
                break;
        }

        if (attackCommand->TargetPtr[i] == NULL)
        {
            someTargetsDied = TRUE;
        }
    }

    if (someTargetsDied)
    {
        dbgMessage("WARNING: Some targets died before order executed");
        clRemoveTargetFromSelection(attackCommand,NULL);  // remove NULL ship pointers from attackCommand
    }

    return attackCommand;
}

/*-----------------------------------------------------------------------------
    Name        : packetSendToCaptain
    Description : sends packet to the captain (if not the captain, actually
                  sends to captain across the network.  If is the captain,
                  fake sends it.
    Inputs      :
    Outputs     :
    Return      : TRUE if packet memory should be freed.
----------------------------------------------------------------------------*/
bool32 packetSendToCaptain(ubyte *packet,udword sizeofPacket)
{
    dbgAssertOrIgnore(((HWPacketHeader *)packet)->type == PACKETTYPE_COMMAND);

    if (playPackets)
    {
        return TRUE;
    }

    if (CaptainNotDefined)
    {
        return TRUE;
    }

    if (recordFakeSendPackets)
    {
        ReceivedCmdPacketCB(packet,sizeofPacket);      // simulate fake transmit
        return TRUE;
    }

    if (IAmCaptain)
    {
        ReceivedCmdPacketCB(packet,sizeofPacket);      // simulate fake transmit to captain
        return TRUE;
    }
    else
    {
        titanSendPointMessage(captainIndex,packet,sizeofPacket);        // send message to captain
        return TRUE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : packetBroadcastSync
    Description : broadcasts the sync packet
    Inputs      :
    Outputs     :
    Return      : TRUE if packet memory should be freed
----------------------------------------------------------------------------*/
bool32 packetBroadcastSync(ubyte *packet,udword sizeofPacket)
{
    dbgAssertOrIgnore(((HWPacketHeader *)packet)->type == PACKETTYPE_SYNC);
    dbgAssertOrIgnore(IAmCaptain);

    if (playPackets)
    {
        return TRUE;
    }

    titanSendBroadcastMessage(packet,sizeofPacket);     // send to everyone else
    ReceivedSyncPacketCB(packet,sizeofPacket);          // and myself too.
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : clPktCommandAlloc / clPktPut16 / clPktPut32 / clPktPutVector / clPktPutSelection
    Description : CAT (M2-R9 recorder-leftovers): the send side of Cataclysm's command packets, the counterpart of the
                  clPkt readers of clProcessSyncPacket below: a 0x1C-byte packet header (type 0xCCCC, the sending
                  player, frame 0, one command at +0x18) and one command (a uword: the type in the low byte, the
                  player in the high byte) whose body is written at the exe's offsets. The exe's blocks are not
                  cleared (the checksum words and the header's last two bytes are the allocator's until the captain's
                  sync packet fills the checksums); here the block is zeroed so a recording's bytes do not depend on
                  the heap. CAT-FIX (M2-R10 tests-leftovers): the HW1-layout senders below (attack, targets, dock,
                  launch, misc, god, RU transfer, autolaunch, alliance, research, drop, create, deterministic build,
                  build, collect, protect: 16 [audit: M2-R10 tests-leftovers: god added, clSendGodSync zeroes too])
                  zero their blocks the same way: their structs' tail padding (NetMiscCommand's
                  last two bytes: 10 + 2n as the exe sizes it) was heap bytes in the .pkts, so two record runs of one
                  game differed there
----------------------------------------------------------------------------*/
static ubyte *clPktCommandAlloc(udword sizeofPacket, udword bodyMax, udword commandType, char *name)
{
    udword size = (bodyMax > sizeofPacket) ? bodyMax : sizeofPacket;
    ubyte *packet = memAlloc(size, name, 0);
    HWPacketHeader *header = (HWPacketHeader *)packet;
    uword command;

    memset(packet, 0, size);
    header->type = PACKETTYPE_COMMAND;
    header->from = (uword)sigsPlayerIndex;
    header->frame = 0;
    header->numberOfCommands = 1;
    command = (uword)(((ubyte)sigsPlayerIndex << 8) | commandType);
    memcpy(packet + sizeof(HWPacketHeader), &command, sizeof(command));
    return packet;
}

static void clPktPut16(ubyte *p, uword v)
{
    memcpy(p, &v, sizeof(v));
}

static void clPktPut32(ubyte *p, udword v)
{
    memcpy(p, &v, sizeof(v));
}

static void clPktPutVector(ubyte *p, vector v)
{
    memcpy(p, &v.x, sizeof(real32));
    memcpy(p + 4, &v.y, sizeof(real32));
    memcpy(p + 8, &v.z, sizeof(real32));
}

// a NetSelection: the count, then each ship's number (the exe writes ShipID.shipNumber, +0x43C of its Ship)
static void clPktPutSelection(ubyte *p, SelectCommand *selectcom)
{
    sdword i;

    clPktPut16(p, (uword)selectcom->numShips);
    for (i = 0; i < selectcom->numShips; i++)
    {
        clPktPut16(p + 2 + 2 * i, (selectcom->ShipPtr[i])->shipID.shipNumber);
    }
}

#define CL_PKT_BODY     (sizeof(HWPacketHeader) + sizeof(HWCommandHeader))     // 0x1E: the command body

/*-----------------------------------------------------------------------------
    Name        : clSendMove
    Description : sends the Move command over the network
    Inputs      : selectcom, the path (type, from, points)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT (M2-R9 recorder-leftovers): clSendMove 0x0044eb20 takes clWrapMove's arguments unchanged (the path type, from,
//      the number of points and the points) and sends 0x124 + 2n bytes: +0 the path type, +4 from, +0x10 the number of
//      points, +0x14 the points (numPoints * 12 bytes copied as given), +0x104 the selection (written after the
//      points). HW1 sent from / to and a ShipID selection. The block is sized for the points too: the callers pass
//      at most CAT_MOVE_MAX_POINTS (21), whose last point the selection overwrites as in the exe; the exe's block
//      would overflow only from 25 points on
void clSendMove(CommandLayer *comlayer,SelectCommand *selectcom,sdword pathType,vector from,sdword numPoints,vector *points)
{
    udword sizeofPacket;
    ubyte *packet;
    ubyte *body;
    sdword i;

    dbgAssertOrIgnore(selectcom->numShips > 0);

    sizeofPacket = 0x124 + 2 * (udword)selectcom->numShips;
    packet = clPktCommandAlloc(sizeofPacket, CL_PKT_BODY + 0x14 + 12 * (udword)((numPoints > 0) ? numPoints : 0),
                               COMMANDTYPE_MOVE, "movepacket");
    body = packet + CL_PKT_BODY;

    clPktPut32(body, (udword)pathType);
    clPktPutVector(body + 4, from);
    clPktPut32(body + 0x10, (udword)numPoints);
    for (i = 0; i < numPoints; i++)
    {
        clPktPutVector(body + 0x14 + 12 * i, points[i]);
    }
    clPktPutSelection(body + 0x104, selectcom);

    if (packetSendToCaptain(packet,sizeofPacket))
    {
        memFree(packet);
    }
}
/*-----------------------------------------------------------------------------
    Name        : clSendMpHyperspace
    Description : sends the Multiplayer hyperspace command over the network
    Inputs      : selectcom,from,to
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT (M2-R9 recorder-leftovers): clSendMpHyperspace 0x0044ed00 sends the move's layout (0x124 + 2n): path type 0,
//      from, one point (the destination) and the selection at +0x104
void clSendMpHyperspace(CommandLayer *comlayer,SelectCommand *selectcom,vector from,vector to)
{
    udword sizeofPacket;
    ubyte *packet;
    ubyte *body;

    dbgAssertOrIgnore(selectcom->numShips > 0);

    sizeofPacket = 0x124 + 2 * (udword)selectcom->numShips;
    packet = clPktCommandAlloc(sizeofPacket, 0, COMMANDTYPE_MP_HYPERSPACE, "hyperspacepacket");
    body = packet + CL_PKT_BODY;

    clPktPut32(body, 0);
    clPktPutVector(body + 4, from);
    clPktPut32(body + 0x10, 1);
    clPktPutVector(body + 0x14, to);
    clPktPutSelection(body + 0x104, selectcom);

    if (packetSendToCaptain(packet,sizeofPacket))
    {
        memFree(packet);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSendAttack
    Description : sends the attack command over the network
    Inputs      : selectcom,attackcom
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clSendAttack(CommandLayer *comlayer,SelectCommand *selectcom,AttackCommand *attackcom)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    NetAttackSelection *attack;
    NetSelection *selection;
    udword numShips = selectcom->numShips;
    udword numShipsToAttack = attackcom->numTargets;
    udword i;

    dbgAssertOrIgnore(numShips > 0);
    dbgAssertOrIgnore(numShipsToAttack > 0);

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeofNetAttackCommand(numShipsToAttack,numShips);

    packet = memAlloc(sizeofPacket,"attackpacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_ATTACK | (sigsPlayerIndex<<8);

    attack = (NetAttackSelection *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));
    attack->numTargets = (uword)numShipsToAttack;
    for (i=0;i<numShipsToAttack;i++)
    {
        SetTargetID(&attack->TargetID[i],attackcom->TargetPtr[i]);
    }

    selection = (NetSelection *)(((ubyte *)attack) + sizeofNetAttackSelection(numShipsToAttack));
    selection->numShips = (uword)numShips;
    for (i=0;i<numShips;i++)
    {
        selection->ShipID[i] = (selectcom->ShipPtr[i])->shipID;
    }

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSendSpecial
    Description : sends the special command over the network
    Inputs      : selectcom,targetcom
    Outputs     :
    Return      :
    Notes       : targetcom may be NULL
----------------------------------------------------------------------------*/
void clSendSpecial(CommandLayer *comlayer,SelectCommand *selectcom,SpecialCommand *targetcom)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    NetTargetsSelection *targets;
    NetSelection *selection;
    sdword numShips = selectcom->numShips;
    sdword numShipsToTarget;
    sdword i;

    dbgAssertOrIgnore(numShips > 0);
    if (targetcom == NULL)
    {
        numShipsToTarget = 0;
    }
    else
    {
        numShipsToTarget = targetcom->numTargets;
    }

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeofNetSpecialCommand(numShipsToTarget,numShips);

    packet = memAlloc(sizeofPacket,"targetspacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_SPECIAL | (sigsPlayerIndex<<8);

    targets = (NetTargetsSelection *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));
    targets->numTargets = (uword)numShipsToTarget;
    for (i=0;i<numShipsToTarget;i++)
    {
        SetTargetID(&targets->TargetID[i],targetcom->TargetPtr[i]);
    }

    selection = (NetSelection *)(((ubyte *)targets) + sizeofNetTargetsSelection(numShipsToTarget));
    selection->numShips = (uword)numShips;
    for (i=0;i<numShips;i++)
    {
        selection->ShipID[i] = (selectcom->ShipPtr[i])->shipID;
    }

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSendFormation
    Description : sends the formation command over the network
    Inputs      : selectcom, formation
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT (M2-R9 recorder-leftovers): clSendFormation 0x0044f690 sends 0x24 + 2n bytes: +0 the formation, +2 the
//      force-field level (clWrapFormation's fourth argument, both as uwords), +4 the selection; it sends for any
//      non-empty selection (HW1 returned below ABSOLUTE_MIN_SHIPS_IN_FORMATION; clFormation tests that)
void clSendFormation(CommandLayer *comlayer,SelectCommand *selectcom,TypeOfFormation formation,sdword catForceFieldLevel)
{
    udword sizeofPacket;
    ubyte *packet;
    ubyte *body;

    if (selectcom->numShips <= 0)
    {
        return;
    }

    sizeofPacket = 0x24 + 2 * (udword)selectcom->numShips;
    packet = clPktCommandAlloc(sizeofPacket, 0, COMMANDTYPE_FORMATION, "formationpacket");
    body = packet + CL_PKT_BODY;

    clPktPut16(body, (uword)formation);
    clPktPut16(body + 2, (uword)catForceFieldLevel);
    clPktPutSelection(body + 4, selectcom);

    if (packetSendToCaptain(packet,sizeofPacket))
    {
        memFree(packet);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSendDock
    Description : sends the Dock command over the network
    Inputs      : selectcom, dockType
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clSendDock(CommandLayer *comlayer,SelectCommand *selectcom,DockType dockType,ShipPtr dockwith)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    NetDockCommand *dock;
    udword numShips = selectcom->numShips;
    udword i;

    dbgAssertOrIgnore(numShips > 0);

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeofNetDockCommand(numShips);

    packet = memAlloc(sizeofPacket,"dockpacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_DOCK | (sigsPlayerIndex<<8);

    dock = (NetDockCommand *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));
    if (dockwith != NULL)
    {
        dock->dockwithID = dockwith->shipID;
    }
    else
    {
        dock->dockwithID.shipNumber = MAX_ID_NUMBER;
    }
    dock->dockType = (uword)dockType;
    dock->selection.numShips = (uword)numShips;
    for (i=0;i<numShips;i++)
    {
        dock->selection.ShipID[i] = (selectcom->ShipPtr[i])->shipID;
    }

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSendLaunchMultipleShips
    Description : sends the Launch multiple ships command over the network
    Inputs      : selectcom, launchFrom
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clSendLaunchMultipleShips(CommandLayer *comlayer,SelectCommand *selectcom,ShipPtr launchFrom)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    NetLaunchMultipleCommand *launch;
    udword numShips = selectcom->numShips;
    udword i;

    dbgAssertOrIgnore(numShips > 0);
    dbgAssertOrIgnore(launchFrom);

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeofNetLaunchMultipleCommand(numShips);

    packet = memAlloc(sizeofPacket,"launchpacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_LAUNCHMULTIPLE | (sigsPlayerIndex<<8);

    launch = (NetLaunchMultipleCommand *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));
    launch->launchfromID = launchFrom->shipID;
    launch->selection.numShips = (uword)numShips;
    for (i=0;i<numShips;i++)
    {
        launch->selection.ShipID[i] = (selectcom->ShipPtr[i])->shipID;
    }

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSendMisc
    Description : sends a miscellaneous command of type miscCommand over the network
    Inputs      : selectcom, miscCommand, miscData
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clSendMisc(CommandLayer *comlayer,SelectCommand *selectcom,uword miscCommand,udword miscData)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    NetMiscCommand *misc;
    udword numShips = selectcom->numShips;
    udword i;

    dbgAssertOrIgnore(numShips > 0);

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeofNetMiscCommand(numShips);

    packet = memAlloc(sizeofPacket,"miscpacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_MISC | (sigsPlayerIndex<<8);

    misc = (NetMiscCommand *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));
    misc->miscCommand = miscCommand;
    misc->miscData = miscData;
    misc->selection.numShips = (uword)numShips;

    for (i=0;i<numShips;i++)
    {
        misc->selection.ShipID[i] = (selectcom->ShipPtr[i])->shipID;
    }

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}


#ifdef GOD_LIKE_SYNC_CHECKING
void clSendGodSync(GodSyncCheckSums *checksumStruct,sdword playIndex,udword type)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    GodSyncCommand *misc;

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader)  + sizeof(GodSyncCommand);

    packet = memAlloc(sizeofPacket,"godpacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_GODSYNC | (sigsPlayerIndex<<8);

    misc = (GodSyncCommand *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));
    misc->type = type;
    misc->frame = universe.univUpdateCounter;
    misc->from=playIndex;

    //copy check
    misc->godChecksum=*checksumStruct;

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}
#endif
void clSendRuTransfer(CommandLayer *comlayer,sdword fromIndex,sdword toIndex, sdword resourceUnits,ubyte flags)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    NetRUTransferferCommand *ruTransfer;

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeof(NetRUTransferferCommand);

    packet = memAlloc(sizeofPacket,"rtranspacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_RUTRANSFER | (sigsPlayerIndex<<8);

    ruTransfer = (NetRUTransferferCommand *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));

    ruTransfer->resourceUnits = resourceUnits;
    ruTransfer->to = (ubyte)toIndex;
    ruTransfer->from = (ubyte)fromIndex;
    ruTransfer->flags = flags;

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}


/*-----------------------------------------------------------------------------
    Name        : clSendAutoLaunch
    Description : sends the autolaunch command over the network (for turning autolaunch on/off)
    Inputs      : OnOff, playerIndex
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clSendAutoLaunch(udword OnOff,udword playerIndex)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    NetAutolaunchCommand *autolaunch;

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeofNetAutolaunchCommand;

    packet = memAlloc(sizeofPacket,"alaunchpacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_AUTOLAUNCH | (sigsPlayerIndex<<8);

    autolaunch = (NetAutolaunchCommand *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));

    autolaunch->OnOff = (uword)OnOff;
    autolaunch->playerIndex = (uword)playerIndex;

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSendSetAlliance
    Description : Sets an alliance based on a player bitmask recieved.
    Inputs      : player bitmask
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void clSendSetAlliance(udword AllianceType, uword playerone, uword playertwo)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    NetAllianceCommand *allianceptr;

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeofNetAllianceCommand;

    packet = memAlloc(sizeofPacket,"aliancepacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_ALLIANCEINFO | (sigsPlayerIndex<<8);

    allianceptr = (NetAllianceCommand *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));

    allianceptr->AllianceMsgType = AllianceType;
    allianceptr->CurAlliance     = playerone;
    allianceptr->NewAlliance     = playertwo;

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}

void clSendResearch(udword type, udword playernum, udword labnum, udword tech)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    NetResearchCommand *researchptr;

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeofNetResearchCommand;

    packet = memAlloc(sizeofPacket,"researchpacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_RESEARCHINFO | (sigsPlayerIndex<<8);

    researchptr = (NetResearchCommand *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));

    researchptr->ResearchMsgType  = (ubyte) type;
    researchptr->playerindex      = (ubyte) playernum;
    researchptr->labnum           = (ubyte) labnum;
    researchptr->tech             = (ubyte) tech;

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}

/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clSendPlayerDropped(udword playerDroppedMask)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    NetPlayerDroppedCommand *PlayerDroppedptr;

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeofNetPlayerDroppedCommand;

    packet = memAlloc(sizeofPacket,"droppacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_PLAYERDROPPED | (sigsPlayerIndex<<8);

    PlayerDroppedptr = (NetPlayerDroppedCommand *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));

    PlayerDroppedptr->playerDroppedMask = playerDroppedMask;
    if (IAmCaptain) PlayerDroppedptr->verify = playerDroppedMask ^ KILLDROPPEDOUTPLAYER_VERIFY;

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSendCreateShip
    Description : sends the create ship command over the network
    Inputs      : shipType, shipRace, playerIndex
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clSendCreateShip(CommandLayer *comlayer,ShipType shipType,ShipRace shipRace,uword playerIndex,ShipPtr creator)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    NetCreateShipCommand *create;

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeofNetCreateShipCommand;

    packet = memAlloc(sizeofPacket,"createpacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_CREATESHIP | (sigsPlayerIndex<<8);

    create = (NetCreateShipCommand *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));

    create->shipType = (uword)shipType;
    create->shipRace = (uword)shipRace;
    create->playerIndex = playerIndex;
    create->creatorID = creator->shipID;

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}

void clSendDeterministicBuild(udword command, CommandLayer* comlayer, sdword numShips,
                              ShipType shipType, ShipRace shipRace,
                              uword playerIndex, ShipPtr creator)
{
    udword sizeofPacket;
    HWPacketHeader* packet;
    HWCommandHeader* commandheader;
    NetDeterministicBuildCommand* create;

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeofNetDeterministicBuildCommand;

    packet = memAlloc(sizeofPacket, "dbuildpacket", 0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader*)(((ubyte*)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_DETERMINISTICBUILD | (sigsPlayerIndex<<8);

    create = (NetDeterministicBuildCommand*)(((ubyte*)commandheader) + sizeof(HWCommandHeader));

    create->subCommand = (ubyte)command;
    create->numShips = (uword)numShips;
    create->shipType = (uword)shipType;
    create->shipRace = (uword)shipRace;
    create->playerIndex = playerIndex;
    create->creatorID = creator->shipID;

    if (packetSendToCaptain((ubyte*)packet, sizeofPacket))
    {
        memFree(packet);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSendBuildShip
    Description : sends the build ship command over the network
    Inputs      : shipType, shipRace, playerIndex
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clSendBuildShip(CommandLayer *comlayer,ShipType shipType,ShipRace shipRace,uword playerIndex,ShipPtr creator)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    NetCreateShipCommand *create;

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeofNetCreateShipCommand;

    packet = memAlloc(sizeofPacket,"buildpacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_BUILDSHIP | (sigsPlayerIndex<<8);

    create = (NetCreateShipCommand *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));

    create->shipType = (uword)shipType;
    create->shipRace = (uword)shipRace;
    create->playerIndex = playerIndex;
    create->creatorID = creator->shipID;

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSendCollectResource
    Description : sends the CollectResource command over the network
    Inputs      : selectcom, resource
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clSendCollectResource(CommandLayer *comlayer,SelectCommand *selectcom,ResourcePtr resource)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    NetCollectResourceCommand *collect;
    udword numShips = selectcom->numShips;
    udword i;

    dbgAssertOrIgnore(numShips > 0);

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeofNetCollectResourceCommand(numShips);

    packet = memAlloc(sizeofPacket,"collectpacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_COLLECTRESOURCE | (sigsPlayerIndex<<8);

    collect = (NetCollectResourceCommand *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));
    if (resource == NULL)
    {
        collect->resourceID.resourceNumber = MAX_ID_NUMBER;
    }
    else
    {
        collect->resourceID = resource->resourceID;
    }
    collect->selection.numShips = (uword)numShips;
    for (i=0;i<numShips;i++)
    {
        collect->selection.ShipID[i] = (selectcom->ShipPtr[i])->shipID;
    }

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSendProtect
    Description : sends protect command over network
    Inputs      : selectcom, protectcom
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clSendProtect(CommandLayer *comlayer,SelectCommand *selectcom,ProtectCommand *protectcom)
{
    udword sizeofPacket;
    HWPacketHeader *packet;
    HWCommandHeader *commandheader;
    NetProtectSelection *protect;
    NetSelection *selection;
    udword numShips = selectcom->numShips;
    udword numShipsToProtect = protectcom->numShips;
    udword i;

    dbgAssertOrIgnore(numShips > 0);
    dbgAssertOrIgnore(numShipsToProtect > 0);

    sizeofPacket = sizeof(HWPacketHeader) + sizeof(HWCommandHeader) + sizeofNetProtectCommand(numShipsToProtect,numShips);

    packet = memAlloc(sizeofPacket,"protectpacket",0);
    memset(packet, 0, sizeofPacket);    // CAT-FIX (M2-R10 tests-leftovers): the struct padding zeroed (clPktCommandAlloc's rule)

    packet->type = PACKETTYPE_COMMAND;
    packet->from = (uword)sigsPlayerIndex;
    packet->frame = 0;
    packet->numberOfCommands = 1;

    commandheader = (HWCommandHeader *)(((ubyte *)packet) + sizeof(HWPacketHeader));
    commandheader->commandType = COMMANDTYPE_PROTECT | (sigsPlayerIndex<<8);

    protect = (NetProtectSelection *)(((ubyte *)commandheader) + sizeof(HWCommandHeader));
    protect->numShips = (uword)numShipsToProtect;
    for (i=0;i<numShipsToProtect;i++)
    {
        protect->ShipID[i] = (protectcom->ShipPtr[i])->shipID;
    }

    selection = (NetSelection *)(((ubyte *)protect) + sizeofNetProtectSelection(numShipsToProtect));
    selection->numShips = (uword)numShips;
    for (i=0;i<numShips;i++)
    {
        selection->ShipID[i] = (selectcom->ShipPtr[i])->shipID;
    }

    if (packetSendToCaptain((ubyte *)packet,sizeofPacket))
    {
        memFree(packet);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clProcessSyncPacket
    Description : Given the sync packet, it parses it and executes any commands
                  inside it by passing them to the command layer.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    CAT (M2-R6 oracle-mode-a): the receive side of Cataclysm's sync packets, as clProcessSyncPacket 0x00450910
    parses them (its jump table at 0x004510f4). The packet header is 0x1C bytes (NetSyncChecksums carries the 4th
    word), the commands follow, each a uword (type in the low byte, the sending player in the high byte) and a body
    whose layout the readers below take from the exe's offsets (memcpy reads: the bodies are only 2-byte aligned).
    Bodies, from the command word's end:
      1 move / 15 multiplayer hyperspace: +0 path type, +4 from, +0x10 number of points, +0x14 the points (the
        hyperspace's destination is the first), +0x104 the selection (0x106 + 2n)
      2 attack / 9 special / 25, 26 special target (flag 1 / 0): the targets (count, 4-byte target IDs), then the
        selection
      3 formation: +0 type, +2 force-field level, +4 selection;  4 dock: +0 dock-with ship ID, +2 purpose, +4 selection
      5 create ship / 8 build ship: type, race, player, creator ID (8)
      6 collect: resource ID, selection;  7 protect: the ships to protect (a selection), then the selection
      10 misc: +0 data (dword), +4 the misc command, +6 the selection (10 + 2n): 0 halt, 1 scuttle, 2 tactics,
        3 kamikaze, 4 parade, 5 link, 6 unlink, 7 Leech action, 8 unleech, 9 module slot (the data's bits name the
        module type and the slot count, tables 0x0072a6d0 / 0x0072a708), 10 Beast system (a one-ship selection),
        11 start the external-build timer
      11 autolaunch (4), 12 launch multiple (launcher ID, selection that may hold ships inside), 13 alliance (8),
      14 RU transfer (8), 16 nothing (no body), 17 player dropped (8), 18 deterministic build (12), 19 research (4)
      20 external-build update: +0 player, +4 progress, +8 mode, +0xC creator ID, +0xE ship ID (16)
      21 point order: +0 the point, +0xC selection;  22 Mimic: +0 mode, +4 type, +8 player, +0xC index, +0x10 selection
      23 Cataclysm research: +0 player, +4 lab, +8 tech, +0xC mode (16);  24 upgrade: +0 cancel, +4 selection
      27 nothing (8)
----------------------------------------------------------------------------*/
static uword clPktU16(const ubyte *p)
{
    uword v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static udword clPktU32(const ubyte *p)
{
    udword v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static vector clPktVector(const ubyte *p)
{
    vector v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static ShipID clPktShipID(const ubyte *p)
{
    ShipID id;
    id.shipNumber = clPktU16(p);
    return id;
}

static udword clPktSizeofNetSelection(const ubyte *p)
{
    return 2 + 2 * (udword)clPktU16(p);
}

static udword clPktSizeofNetTargets(const ubyte *p)
{
    return 2 + 4 * (udword)clPktU16(p);
}

// a NetSelection / NetAttackSelection copied out of the packet, so the converters read aligned data
static SelectCommand *clPktSelection(const ubyte *p, bool32 considerInsideShips, uword from)
{
    udword size = clPktSizeofNetSelection(p);
    NetSelection *copy = memAlloc((sdword)(size < sizeof(NetSelection) ? sizeof(NetSelection) : size), "netselcopy", Pyrophoric);
    SelectCommand *selectcom;

    memcpy(copy, p, size);
    selectcom = convertNetSelectionToSelectCommand(copy, considerInsideShips, from);
    memFree(copy);
    return selectcom;
}

static AttackCommand *clPktTargets(const ubyte *p)
{
    udword size = clPktSizeofNetTargets(p);
    NetAttackSelection *copy = memAlloc((sdword)(size < sizeof(NetAttackSelection) ? sizeof(NetAttackSelection) : size), "nettgtcopy", Pyrophoric);
    AttackCommand *attackcom;

    memcpy(copy, p, size);
    attackcom = convertNetAttackSelectionToAttackCommand(copy);
    memFree(copy);
    return attackcom;
}

// the misc command 9's data bits: the module type (0x0072a6d0: 7 {type, bit} pairs) and the slot count (0x0072a708: 3 pairs)
static const sdword clPktModuleTypeBits[7][2] = { {0x13, 0x01}, {0x15, 0x02}, {0x16, 0x04}, {0x17, 0x08}, {0x18, 0x10}, {0x1A, 0x20}, {0x1F, 0x40} };
static const sdword clPktModuleCountBits[3][2] = { {0, 0x80}, {1, 0x100}, {2, 0x200} };

void clProcessSyncPacket(CommandLayer *comlayer,ubyte *packet,udword sizeofPacket)
{
    const ubyte *body;
    udword numberOfCommandsLeft;
    uword commandType;
    uword from;
    SelectCommand *selectcom;
    SelectCommand *selectcom2;
    AttackCommand *targets;
    udword n, k, data;
    sdword moduleType, moduleCount;
    vector point;

    dbgAssertOrIgnore(((HWPacketHeader *)packet)->type == PACKETTYPE_SYNC);

    if (!playPackets)
    if (((HWPacketHeader *)packet)->frame != receivedPacketNumber)
    {
        dbgMessagef("Expected pkt %d got %d",receivedPacketNumber,((HWPacketHeader *)packet)->frame);
        pktSyncErr++;
    }
    receivedPacketNumber++;

    if (recordPackets)
    {
        if (OrigRecordPacketFileName[0])
        {
            recPackRecordPacketFilename(packet,sizeofPacket,OrigRecordPacketFileName);
        }

        recPackRecordPacket(packet,sizeofPacket);
    }
    netcheckShow((HWPacketHeader *)packet);

#ifdef DEBUG_GAME_STATS
    if(statLogOn)
    {
        gameStatsUpdateLogFile(universe.univUpdateCounter);
    }
#endif

    numberOfCommandsLeft = (udword)((HWPacketHeader *)packet)->numberOfCommands;   // CAT: +0x18

    body = packet + sizeof(HWPacketHeader);                                     // CAT: +0x1C

    while (numberOfCommandsLeft > 0)
    {
#if HWI_ORACLE
        const ubyte *cmdstart = body;       // HWI-ORACLE (M2-R8 oracle-rung-physics): /oracleShipDump's 'C' line
#endif
        commandType = clPktU16(body);
        from = (commandType >> 8) & 255;
        commandType &= 255;
        body += sizeof(HWCommandHeader);

        switch (commandType)
        {
            case COMMANDTYPE_MOVE:
                selectcom = clPktSelection(body + 0x104, FALSE, from);
                n = clPktU32(body + 0x10);
                {
                    vector points[CAT_NETMOVE_MAX_POINTS];
                    memset(points, 0, sizeof(points));
                    memcpy(points, body + 0x14, sizeof(vector) * ((n < CAT_NETMOVE_MAX_POINTS) ? n : CAT_NETMOVE_MAX_POINTS));
                    clMove(comlayer, selectcom, (sdword)clPktU32(body), clPktVector(body + 4), (sdword)n, points);
                }
                memFree(selectcom);
                body += 0x106 + 2 * (udword)clPktU16(body + 0x104);
                break;

            case COMMANDTYPE_MP_HYPERSPACE:
                selectcom = clPktSelection(body + 0x104, FALSE, from);
                clMpHyperspace(comlayer, selectcom, clPktVector(body + 4), clPktVector(body + 0x14));
                memFree(selectcom);
                body += 0x106 + 2 * (udword)clPktU16(body + 0x104);
                break;

            case COMMANDTYPE_ATTACK:
                n = clPktSizeofNetTargets(body);
                targets = clPktTargets(body);
                selectcom = clPktSelection(body + n, FALSE, from);
                clAttack(comlayer, selectcom, targets);
                memFree(targets);
                memFree(selectcom);
                body += n + clPktSizeofNetSelection(body + n);
                break;

            case COMMANDTYPE_SPECIAL:
            case CAT_COMMANDTYPE_SPECIALTARGET:
            case CAT_COMMANDTYPE_SPECIALTARGET0:
                n = clPktSizeofNetTargets(body);
                selectcom = clPktSelection(body + n, FALSE, from);
                targets = (clPktU16(body) != 0) ? clPktTargets(body) : NULL;
                if (commandType == COMMANDTYPE_SPECIAL)
                {
                    clSpecial(comlayer, selectcom, targets);
                }
                else
                {
                    cat_clSpecialTarget(comlayer, selectcom, targets, (commandType == CAT_COMMANDTYPE_SPECIALTARGET));
                }
                memFree(selectcom);
                if (targets != NULL) memFree(targets);
                body += n + clPktSizeofNetSelection(body + n);
                break;

            case COMMANDTYPE_FORMATION:
                selectcom = clPktSelection(body + 4, FALSE, from);
                clFormation(comlayer, selectcom, (TypeOfFormation)clPktU16(body), (sdword)clPktU16(body + 2));
                memFree(selectcom);
                body += 4 + clPktSizeofNetSelection(body + 4);
                break;

            case COMMANDTYPE_DOCK:
                selectcom = clPktSelection(body + 4, FALSE, from);
                clDock(comlayer, selectcom, (DockType)clPktU16(body + 2), ShipIDtoShip(clPktShipID(body), FALSE));
                memFree(selectcom);
                body += 4 + clPktSizeofNetSelection(body + 4);
                break;

            case COMMANDTYPE_CREATESHIP:
                clCreateShip(comlayer, (ShipType)clPktU16(body), (ShipRace)clPktU16(body + 2), clPktU16(body + 4),
                             ShipIDtoShip(clPktShipID(body + 6), FALSE));
                body += 8;
                break;

            case COMMANDTYPE_BUILDSHIP:
                clBuildShip(comlayer, (ShipType)clPktU16(body), (ShipRace)clPktU16(body + 2), clPktU16(body + 4),
                            ShipIDtoShip(clPktShipID(body + 6), FALSE));
                body += 8;
                break;

            case COMMANDTYPE_COLLECTRESOURCE:
                {
                    ResourceID resourceID;
                    resourceID.resourceNumber = clPktU16(body);
                    selectcom = clPktSelection(body + 2, FALSE, from);
                    clCollectResource(comlayer, selectcom, ResourceIDtoResourcePtr(resourceID));
                    memFree(selectcom);
                    body += 2 + clPktSizeofNetSelection(body + 2);
                }
                break;

            case COMMANDTYPE_PROTECT:
                n = clPktSizeofNetSelection(body);
                selectcom2 = clPktSelection(body, FALSE, from);         // the ships to protect
                selectcom = clPktSelection(body + n, FALSE, from);
                clProtect(comlayer, selectcom, (ProtectCommand *)selectcom2);
                memFree(selectcom2);
                memFree(selectcom);
                body += n + clPktSizeofNetSelection(body + n);
                break;

            case COMMANDTYPE_MISC:
                data = clPktU32(body);
                selectcom = clPktSelection(body + 6, FALSE, from);
                switch (clPktU16(body + 4))
                {
                    case MISCCOMMAND_HALT:      clHalt(comlayer, selectcom); break;
                    case MISCCOMMAND_SCUTTLE:   clScuttle(comlayer, selectcom); break;
                    case MISCCOMMAND_TACTICS:   clSetTactics(comlayer, selectcom, (TacticsType)data); break;
                    case MISCCOMMAND_KAMIKAZE:  clSetKamikaze(comlayer, selectcom); break;
                    case MISCCOMMAND_PARADE:    clSetMilitaryParade(comlayer, selectcom); break;
                    case CAT_MISCCOMMAND_LINK:          cat_clLink(comlayer, selectcom); break;
                    case CAT_MISCCOMMAND_UNLINK:        cat_clUnlink(comlayer, selectcom); break;
                    case CAT_MISCCOMMAND_LEECHACTION:   cat_clSetLeechAction(comlayer, selectcom, (ubyte)data); break;
                    case CAT_MISCCOMMAND_UNLEECH:       cat_clUnleech(comlayer, selectcom); break;
                    case CAT_MISCCOMMAND_MODULESLOT:
                        moduleType = 0;
                        moduleCount = -1;
                        for (k = 0; k < 7; k++)
                        {
                            if (clPktModuleTypeBits[k][1] & data) moduleType = clPktModuleTypeBits[k][0];
                        }
                        for (k = 0; k < 3; k++)
                        {
                            if (clPktModuleCountBits[k][1] & data) moduleCount = clPktModuleCountBits[k][0];
                        }
                        cat_clSetModuleSlot(comlayer, selectcom, moduleType, moduleCount);
                        break;
                    case CAT_MISCCOMMAND_BEASTSYSTEM:
                        if (selectcom->numShips == 1)
                        {
                            cat_clSetBeastSystem(comlayer, selectcom->ShipPtr[0], (sdword)data);
                        }
                        break;
                    case CAT_MISCCOMMAND_EXTERNALBUILDTIMER:
                        cat_clStartExternalBuildTimer(comlayer, selectcom);
                        break;
                    default:
                        break;                  // CAT: the exe ignores other numbers (jump table bound 0xb)
                }
                memFree(selectcom);
                body += 10 + 2 * (udword)clPktU16(body + 6);
                break;

            case COMMANDTYPE_AUTOLAUNCH:
                clAutoLaunch((udword)clPktU16(body), (udword)clPktU16(body + 2));
                body += 4;
                break;

            case COMMANDTYPE_LAUNCHMULTIPLE:
                selectcom = clPktSelection(body + 2, TRUE, from);
                clLaunchMultipleShips(comlayer, selectcom, ShipIDtoShip(clPktShipID(body), FALSE));
                memFree(selectcom);
                body += 2 + clPktSizeofNetSelection(body + 2);
                break;

            case COMMANDTYPE_ALLIANCEINFO:
                clSetAlliance(clPktU32(body), clPktU16(body + 4), clPktU16(body + 6));
                body += 8;
                break;

            case COMMANDTYPE_RUTRANSFER:
                clRUTransfer(comlayer, (sdword)body[0], (sdword)body[1], (sdword)clPktU32(body + 4), body[2]);
                body += 8;
                break;

            case COMMANDTYPE_PLAYERDROPPED:
                clPlayerDropped(clPktU32(body), clPktU32(body + 4));
                body += 8;
                break;

            case COMMANDTYPE_DETERMINISTICBUILD:
                clDeterministicBuild((udword)body[0], comlayer, (sdword)clPktU16(body + 2), (ShipType)clPktU16(body + 4),
                                     (ShipRace)clPktU16(body + 6), clPktU16(body + 8), ShipIDtoShip(clPktShipID(body + 10), FALSE));
                body += 12;
                break;

            case COMMANDTYPE_RESEARCHINFO:
                clSetResearch((udword)body[0], (udword)body[1], (udword)body[2], (udword)body[3]);
                body += 4;
                break;

            case CAT_COMMANDTYPE_EXTERNALBUILDUPDATE:
                {
                    real32 progress;
                    data = clPktU32(body + 4);
                    memcpy(&progress, &data, sizeof(progress));
                    cat_clExternalBuildUpdate(comlayer, clPktU16(body), ShipIDtoShip(clPktShipID(body + 0xC), FALSE),
                                              ShipIDtoShip(clPktShipID(body + 0xE), FALSE), progress, (sdword)clPktU32(body + 8));
                }
                body += 16;
                break;

            case CAT_COMMANDTYPE_POINTORDER:
                selectcom = clPktSelection(body + 0xC, FALSE, from);
                point = clPktVector(body);
                cat_clPointOrder(comlayer, selectcom, &point);
                memFree(selectcom);
                body += 0xC + clPktSizeofNetSelection(body + 0xC);
                break;

            case CAT_COMMANDTYPE_MIMIC:
                selectcom = clPktSelection(body + 0x10, FALSE, from);
                cat_clMimic(comlayer, selectcom, clPktU32(body), clPktU32(body + 4), clPktU32(body + 8), clPktU32(body + 0xC));
                memFree(selectcom);
                body += 0x10 + clPktSizeofNetSelection(body + 0x10);
                break;

            case CAT_COMMANDTYPE_RESEARCH:
                cat_clResearch(comlayer, clPktU16(body), clPktU32(body + 4), clPktU32(body + 8), clPktU32(body + 0xC));
                body += 16;
                break;

            case CAT_COMMANDTYPE_UPGRADE:
                selectcom = clPktSelection(body + 4, FALSE, from);
                cat_clUpgradeShips(comlayer, selectcom, (bool32)clPktU32(body));
                memFree(selectcom);
                body += 4 + clPktSizeofNetSelection(body + 4);
                break;

            case CAT_COMMANDTYPE_NOTHING27:
                body += 8;                          // CAT: an empty handler in the exe (0x00450dd5 calls a nullsub)
                break;

            default:
                // CAT: 16 and unknown numbers: the exe goes on with the next command word (16 has no handler and no
                //      body in Cataclysm; HW1's GODSYNC is gone)
                break;
        }
#if HWI_ORACLE
        oracleShipDumpCommand((udword)((HWPacketHeader *)packet)->frame,
                              (udword)((HWPacketHeader *)packet)->numberOfCommands - numberOfCommandsLeft,
                              (udword)commandType, (udword)from, cmdstart, (udword)(body - cmdstart));
#endif

        numberOfCommandsLeft--;
    }
    (void)sizeofPacket;
}

#define LastSyncPktsQ_NUMBER 1024
#define LastSyncPktsQ_MASK   1023

typedef struct
{
    udword frame;
    HWPacketHeader *packet;
    udword length;
} CircularQEntry;

typedef struct
{
    udword head;
    CircularQEntry circularQEntry[LastSyncPktsQ_NUMBER];
} LastSyncPktsQ;

LastSyncPktsQ lastSyncPktsQ;

void InitLastSyncPktsQ(void)
{
    memset(&lastSyncPktsQ,0,sizeof(lastSyncPktsQ));
}

void CloseLastSyncPktsQ(void)
{
    sdword i;

    for (i=0;i<LastSyncPktsQ_NUMBER;i++)
    {
        if (lastSyncPktsQ.circularQEntry[i].packet)
        {
            memFree(lastSyncPktsQ.circularQEntry[i].packet);
        }
    }
    memset(&lastSyncPktsQ,0,sizeof(lastSyncPktsQ));
}

void ResetLastSyncPktsQ(void)
{
    CloseLastSyncPktsQ();
    InitLastSyncPktsQ();
}

void EnterIntoLastSyncPktsQ(udword frame,HWPacketHeader *packet,udword length)
{
    CircularQEntry *entry = &lastSyncPktsQ.circularQEntry[lastSyncPktsQ.head];

    if (entry->packet)
    {
        memFree(entry->packet);
    }

    dbgAssertOrIgnore(packet);
    entry->frame = frame;
    entry->packet = packet;
    entry->length = length;

    lastSyncPktsQ.head = (lastSyncPktsQ.head+1) & LastSyncPktsQ_MASK;
}

bool32 GetSyncPktFromLastSyncPktsQ(udword frame,HWPacketHeader **packet,udword *size)
{
    sdword i;

    dbgAssertOrIgnore(frame != 0);
    for (i=0;i<LastSyncPktsQ_NUMBER;i++)
    {
        if (lastSyncPktsQ.circularQEntry[i].frame == frame)
        {
            *packet = lastSyncPktsQ.circularQEntry[i].packet;
            *size = lastSyncPktsQ.circularQEntry[i].length;
            return TRUE;
        }
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : clWaitSyncPacket
    Description : Waits for a sync packet.  If one is not present, returns FALSE.
                  If one is present, it processes the sync packet.
    Inputs      :
    Outputs     :
    Return      : TRUE if sync packet received and processed.  FALSE if
                  no sync packet present yet.
----------------------------------------------------------------------------*/
WaitPacketStatus clWaitSyncPacket(CommandLayer *comlayer)
{
    udword numPackets;
    udword sizeofPacket;
    ubyte *packet;
    ubyte *copypacket;

    if (playPackets)
    {
        copypacket = recPackPlayGetNextPacket(&sizeofPacket);
        if (copypacket == NULL)
        {
            return NO_PACKET;
        }
        clProcessSyncPacket(comlayer,copypacket,sizeofPacket);
        memFree(copypacket);
        return PACKET_READY;
    }

    if (receiveSyncPacketsFrom == -1)
    {
        return NO_PACKET;       // we shouldn't receive any packets from anyone till we find out who to receive from
    }

    if (explicitlyRequestingPackets)
    {
        LockQueue(&ProcessRequestedSyncPktQ);
        numPackets = queueNumberEntries(ProcessRequestedSyncPktQ);
        if (numPackets == 0)
        {
            UnLockQueue(&ProcessRequestedSyncPktQ);
            return NO_PACKET;
        }
        else
        {
            sizeofPacket = HWDequeue(&ProcessRequestedSyncPktQ,&packet);       // actually HWDequeue it
            dbgAssertOrIgnore(sizeofPacket > 0);
            copypacket = memAlloc(sizeofPacket,"cp(copypacket)",Pyrophoric);
            memcpy(copypacket,packet,sizeofPacket);
            UnLockQueue(&ProcessRequestedSyncPktQ);

            dbgAssertOrIgnore(((HWPacketHeader *)copypacket)->type == PACKETTYPE_REQUESTEDSYNC);
            ((HWPacketHeader *)copypacket)->type = PACKETTYPE_SYNC;

            if (((HWPacketHeader *)copypacket)->frame == explicitlyRequestingTo)
                explicitlyRequestingPackets = FALSE;
            else if (((HWPacketHeader *)copypacket)->frame == ((udword)-1))
            {
                explicitlyRequestingPackets = FALSE;        // we received fake sync packet because the one we wanted
                                                            // wasn't present.
                memFree(copypacket);
                return NO_PACKET;
            }

            goto gotsyncpkt;
        }
    }

    LockQueue(&ProcessSyncPktQ);
    numPackets = queueNumberEntries(ProcessSyncPktQ);
    if (numPackets == 0)
    {
        UnLockQueue(&ProcessSyncPktQ);
        return NO_PACKET;
    }
    else
    {
        sizeofPacket = Peekqueue(&ProcessSyncPktQ,&packet);
        dbgAssertOrIgnore(sizeofPacket > 0);
        for (;;)
        {
            if (((HWPacketHeader *)packet)->from != receiveSyncPacketsFrom)
            {
                goto getnextpacket;
            }

            if (((HWPacketHeader *)packet)->frame < receivedPacketNumber)
            {
                dbgMessagef("Discarding packet %d already got it",((HWPacketHeader *)packet)->frame);
                goto getnextpacket;
            }

            if (((HWPacketHeader *)packet)->frame == receivedPacketNumber)
            {
                sizeofPacket = HWDequeue(&ProcessSyncPktQ,&packet);       // actually HWDequeue it
                dbgAssertOrIgnore(sizeofPacket > 0);
                break;
            }

            // frame is > receivedPacketNumber, don't HWDequeue it,
            if (!explicitlyRequestingPackets)       // and request packets receivedPacketNumber..frame-1 (but only once)
            {
                explicitlyRequestingFrom = receivedPacketNumber;
                explicitlyRequestingTo = ((HWPacketHeader *)packet)->frame - 1;

                UnLockQueue(&ProcessSyncPktQ);

                SendRequestSyncPkts(explicitlyRequestingFrom,explicitlyRequestingTo);
                explicitlyRequestingPackets = TRUE;
            }
            else
            {
                UnLockQueue(&ProcessSyncPktQ);
            }
            return NO_PACKET;

getnextpacket:
            sizeofPacket = HWDequeue(&ProcessSyncPktQ,&packet);       // actually HWDequeue it
            dbgAssertOrIgnore(sizeofPacket > 0);
            numPackets = queueNumberEntries(ProcessSyncPktQ);
            if (numPackets == 0)
            {
                UnLockQueue(&ProcessSyncPktQ);
                return NO_PACKET;
            }
            sizeofPacket = Peekqueue(&ProcessSyncPktQ,&packet);
            dbgAssertOrIgnore(sizeofPacket > 0);
        }

        copypacket = memAlloc(sizeofPacket,"cp(copypacket)",Pyrophoric);
        memcpy(copypacket,packet,sizeofPacket);
        UnLockQueue(&ProcessSyncPktQ);
gotsyncpkt:
        clProcessSyncPacket(comlayer,copypacket,sizeofPacket);
        EnterIntoLastSyncPktsQ(((HWPacketHeader *)copypacket)->frame,(HWPacketHeader *)copypacket,sizeofPacket);  // last sync packets may be used to recover game if this player takes over as captain
        if (numPackets > HIGH_WATER_MARK)
        {
            return TOO_MANY_PACKETS;
        }
        else
        {
            return PACKET_READY;
        }
    }
}

typedef struct
{
    char *qdata;
    udword qsizeof;
} QInfo;

/*-----------------------------------------------------------------------------
    Name        : captainServerTask
    Description : This is the captain server task, which every so many Hz
                  takes any command packets received, and merges them into
                  a sync packet which it transmits.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
DEFINE_TASK(captainServerTask)
{
    static QInfo *qinfos;
    static QInfo *curqinfo;
    static udword qTotalNumberEntries;
    static udword numCommands;
    static udword j;
    static HWPacketHeader *packet;
    static HWPacketHeader *thispacket;
    static ubyte *curPacketPtr;
    static udword packetlength;
    static udword datalength;
    static udword totalCommands;

    taskBegin;

    taskYield(0);

    for(;;)
    {
        if ( (recordFakeSendPackets) ||
            ((multiPlayerGame) && (gameIsRunning) && (IAmCaptain) && (multiPlayerGameUnderWay)) )
        {
            if (playPackets|recordFakeSendPackets)
            {
                if (universePause)
                {
                    goto donecap;
                }
            }

            numCommands = 0;

            LockQueue(&ProcessCmdPktQ);
            qTotalNumberEntries = queueNumberEntries(ProcessCmdPktQ);
            if (qTotalNumberEntries == 0)
            {
                qinfos = NULL;
            }
            else
            {
                qinfos = memAlloc(sizeof(QInfo)*qTotalNumberEntries,"qinfos",0);
            }

            while (queueNumberEntries(ProcessCmdPktQ) > 0)
            {
                curqinfo = &qinfos[numCommands];
                curqinfo->qsizeof = HWDequeue(&ProcessCmdPktQ,(ubyte **)&curqinfo->qdata);
                dbgAssertOrIgnore(curqinfo->qsizeof > 0);
                dbgAssertOrIgnore(curqinfo->qdata != NULL);
                dbgAssertOrIgnore(numCommands < qTotalNumberEntries);
                numCommands++;
            }

            if (numCommands == 0)
            {
                packetlength = sizeof(HWPacketHeader);
                packet = memAlloc(packetlength,"emptysyncpkt",0);

                packet->numberOfCommands = 0;
            }
            else if (numCommands == 1)
            {
                packetlength = qinfos[0].qsizeof;
                packet = memAlloc(packetlength,"onesync",0);
                memcpy(packet,qinfos[0].qdata,packetlength);

                dbgAssertOrIgnore(packet->type == PACKETTYPE_COMMAND);
                packet->numberOfCommands = 1;
            }
            else
            {
                // received more than 1 command packet, so we must concatenate them into one sync packet
                packetlength = sizeof(HWPacketHeader);
                totalCommands = 0;
                for (j=0;j<numCommands;j++)
                {
                    thispacket = (HWPacketHeader *)qinfos[j].qdata;
                    datalength = qinfos[j].qsizeof - sizeof(HWPacketHeader);

                    dbgAssertOrIgnore(thispacket->type == PACKETTYPE_COMMAND);
                    dbgAssertOrIgnore(datalength > 0);
                    dbgAssertOrIgnore(thispacket->numberOfCommands > 0);

                    packetlength += datalength;
                    totalCommands += thispacket->numberOfCommands;
                }

                packet = memAlloc(packetlength,"concatsyncpkt",0);

                packet->type = PACKETTYPE_SYNC;
                packet->numberOfCommands = (uword)totalCommands;

                curPacketPtr = ((ubyte *)packet) + sizeof(HWPacketHeader);

                for (j=0;j<numCommands;j++)
                {
                    thispacket = (HWPacketHeader *)qinfos[j].qdata;
                    datalength = qinfos[j].qsizeof - sizeof(HWPacketHeader);

                    memcpy(curPacketPtr,((ubyte *)thispacket) + sizeof(HWPacketHeader),datalength);
                    curPacketPtr += datalength;
                }
            }
            if (qinfos != NULL)
            {
                memFree(qinfos);
            }
            UnLockQueue(&ProcessCmdPktQ);

            packet->type = PACKETTYPE_SYNC;
            packet->from = (uword)sigsPlayerIndex;
            packet->frame = sentPacketNumber++;
            // CAT-FIX (M2-R10 tests-leftovers): the header's two padding bytes after numberOfCommands (+0x1A .. +0x1B)
            //      are zeroed. captainServerTask 0x00451580 writes only the uword count at +0x18 in its three branches
            //      (the empty packet's memAllocFunction(0x1c), the single command packet's copy, the concatenation), so
            //      the exe sends and records heap bytes there (GOG's .pkts: 2,535 of 4,800 packets nonzero in 10-17,
            //      1,159 of 1,763 in 10-18 [audit: M2-R10 tests-leftovers: the 10-18 count added, pkts_probe.py]);
            //      nothing reads them, and zero makes two record runs' .pkts byte-identical
            memset((ubyte *)packet + offsetof(HWPacketHeader, numberOfCommands) + sizeof(packet->numberOfCommands), 0,
                   sizeof(HWPacketHeader) - offsetof(HWPacketHeader, numberOfCommands) - sizeof(packet->numberOfCommands));
#if SYNC_CHECK
            netcheckFillInChecksum(packet);
#endif

            if (recordFakeSendPackets)
            {
                ReceivedSyncPacketCB((ubyte *)packet,packetlength);          // and myself too.
                memFree(packet);
            }
            else
            {
                if (packetBroadcastSync((ubyte *)packet,packetlength))
                {
                    memFree(packet);
                }
            }
donecap:;
        }
        taskYield(0);
    }

    taskEnd;
}

/*-----------------------------------------------------------------------------
    Name        : SendTransferCaptaincyPacket
    Description : sends a TransferCaptaincyPacket of subtype subtype with custominfo
    Inputs      : playerIndex,  >= 0 for player, -1 for broadcast, -2 for broadcast including myself
                  subtype, custominfo
    Outputs     :
    Return      : none
----------------------------------------------------------------------------*/
void SendTransferCaptaincyPacket(sdword playerIndex,uword subtype,udword misc,CaptaincyCustomInfo *custominfo)
{
    TransferCaptaincyPacket packet;
    sdword size;

    if (sigsNumPlayers < 2)
    {
        return;
    }

    packet.packetheader.type = PACKETTYPE_TRANSFERCAPTAINCY;
    packet.packetheader.from = (uword)sigsPlayerIndex;
    packet.packetheader.frame = misc;
    packet.packetheader.numberOfCommands = 0;

    dbgAssertOrIgnore(subtype < XFERCAP_NUM);
    packet.subtype = subtype;
    if (custominfo)
    {
        packet.custominfo = *custominfo;
        packet.subtype |= XFERCAP_CUSTOMINFO_PRESENT;
        size = sizeof(TransferCaptaincyPacket);
    }
    else
    {
        size = sizeof(TransferCaptaincyPacketSmall);
    }

    if (playerIndex >= 0)
    {
        dbgAssertOrIgnore(playerIndex < sigsNumPlayers);
        titanAnyoneSendPointMessage(playerIndex,(ubyte *)&packet,size);
    }
    else
    {
        // -1 or lower
        titanAnyoneSendBroadcastMessage((ubyte *)&packet,size);

        if (playerIndex == -2)
        {
            // even send to myself!
            ReceivedTransferCaptaincyPacketCB((ubyte *)&packet,size);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : SendNonCaptainReadyPacket
    Description : sends the "Ready to play packet (I am not the captain)" packet to
                  indicate user is ready to play.
    Inputs      : playerIndex
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SendNonCaptainReadyPacket(udword playerIndex)
{
    HWPacketHeader packet;

    dbgAssertOrIgnore(!IAmCaptain);
    dbgAssertOrIgnore(!CaptainNotDefined);

    packet.type = PACKETTYPE_NONCAPTAINREADY;
    packet.from = (uword)playerIndex;
    packet.frame = playerIndex;
    packet.numberOfCommands = 0;

    titanSendPointMessage(captainIndex,(ubyte *)&packet,sizeof(HWPacketHeader));
}

/*-----------------------------------------------------------------------------
    Name        : SendDroppingOutOfLoad
    Description : sends the "I'm dropping out of loading" packet to
                  everyone else
    Inputs      : my playerIndex
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SendDroppingOutOfLoad(udword playerIndex)
{
    HWPacketHeader packet;

    packet.type = PACKETTYPE_DROPPINGOUTOFLOAD;
    packet.from = (uword)playerIndex;
    packet.frame = playerIndex;
    packet.numberOfCommands = 0;

    titanSendBroadcastMessage((ubyte *)&packet,sizeof(HWPacketHeader));
}

/*-----------------------------------------------------------------------------
    Name        : SendCheatDetect
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SendCheatDetect(void)
{
    HWPacketHeader packet;

    packet.type = PACKETTYPE_CHEATDETECT;
    packet.from = (uword)sigsPlayerIndex;
    packet.frame = blobSyncErrFrame;
    packet.numberOfCommands = 0;

    titanSendBroadcastMessage((ubyte *)&packet,sizeof(HWPacketHeader));
}

/*-----------------------------------------------------------------------------
    Name        : SendInGameQuittingPacket
    Description : sends the "In Game I'm quitting" packet to
                  everyone else
    Inputs      : my playerIndex
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SendInGameQuittingPacket(udword playerIndex)
{
    HWPacketHeader packet;

    packet.type = PACKETTYPE_INGAMEQUITING;
    packet.from = (uword)playerIndex;
    packet.frame = playerIndex;
    packet.numberOfCommands = 0;

    titanSendBroadcastMessage((ubyte *)&packet,sizeof(HWPacketHeader));
}

void SendRequestSyncPkts(udword frompacketnum,udword topacketnum)
{
    RequestSyncPacketsPacket packet;

    dbgAssertOrIgnore(!IAmCaptain);
    dbgAssertOrIgnore(!CaptainNotDefined);

    dbgAssertOrIgnore(topacketnum >= frompacketnum);

    packet.packetheader.type = PACKETTYPE_REQUESTSYNCPKTS;
    packet.packetheader.from = (uword)sigsPlayerIndex;
    packet.packetheader.frame = frompacketnum;
    packet.packetheader.numberOfCommands = 0;
    packet.topacketnum = topacketnum;

    titanSendPointMessage(captainIndex,(ubyte *)&packet,sizeof(RequestSyncPacketsPacket));
}

void SendLagPacket(udword to, ubyte *packet)
{
    titanSendPointMessage(to, packet, sizeof(LagPacket));
}

/*-----------------------------------------------------------------------------
    Name        : SendHorseRacePacket
    Description : Sends A packet Update Regarding a players horse race status

    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SendHorseRacePacket(ubyte *packet,udword sizeofpacket)
{
    if(!multiPlayerGame) return;
    if (CaptainNotDefined) return;
    if(IAmCaptain)
    {
        titanSendBroadcastMessage(packet,sizeofpacket);         // send to everyone else
        recievedHorsePacketCB(packet,sizeofpacket);     // I receive it too
    }
    else
    {
        titanSendPointMessage(captainIndex,packet,sizeofpacket);
    }
}
/*-----------------------------------------------------------------------------
    Name        : BroadCastHorseRacePacket
    Description : Sends HorseRace status to EVERYONE except captain about EVERYONE

    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

void BroadCastHorseRacePacketFromCaptain(ubyte *packet, udword sizeofpacket)
{
    titanSendBroadcastMessage(packet,sizeofpacket);
}

/*-----------------------------------------------------------------------------
    Name        : SendChatPacketPacket
    Description : Sends a packet to all users flagged as TRUE with a packet of CHAT data

    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SendChatPacketPacket(ChatPacket *packet, udword sizeofpacket,udword users)
{
    sdword i;

    if(!multiPlayerGame) return;
    if (CaptainNotDefined)
    {
        return;
    }

    dbgAssertOrIgnore(!(PLAYER_MASK(sigsPlayerIndex) & users));        //shouldn't be sending a message to local player...thats dumb

    packet->bounced = FALSE;

    if(IAmCaptain)
    {
        for (i=0;i<sigsNumPlayers;i++)
        {
            if (i != captainIndex)
            {
                if (bitTest(packet->users,PLAYER_MASK(i)))
                {
                    titanSendPointMessage(i, (ubyte *)packet, sizeofpacket);
                }
            }
        }
    }
    else
    {
        titanSendPointMessage(captainIndex,(ubyte *)packet,sizeofpacket);
    }
}

void BroadCastChatPacketFromCapatain(ChatPacket *packet, udword sizeofpacket)
{
    sdword i;

    if (CaptainNotDefined)
    {
        return;
    }

    dbgAssertOrIgnore(IAmCaptain);

    if (packet->bounced)
    {
        goto justreceiveit;
    }
    packet->bounced = TRUE;

    for (i=0;i<sigsNumPlayers;i++)
    {
        if (i != captainIndex)
        {
            if (bitTest(packet->users,PLAYER_MASK(i)))
            {
                titanSendPointMessage(i, (ubyte *)packet, sizeofpacket);
            }
        }
    }

justreceiveit:
    if (bitTest(packet->users,PLAYER_MASK(captainIndex)))
    {
        recievedChatPacketCB((ubyte *)packet, sizeofpacket);
    }

}

/*-----------------------------------------------------------------------------
    Name        : netCheck
    Description : prints various network statistics
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void netCheck(void)
{
    udword rsyncpkts;
    udword rcmdpkts;
    udword rsyncoverruns;
    udword rcmdoverruns;

    if (unknownPackets != printedUnknownPackets)
    {
        printedUnknownPackets = unknownPackets;
        dbgMessagef("Unknown packets %d",printedUnknownPackets);
    }

    rsyncpkts = queueNumberEntries(ProcessSyncPktQ);
    rcmdpkts = queueNumberEntries(ProcessCmdPktQ);
    rsyncoverruns = queueNumberOverruns(ProcessSyncPktQ);
    rcmdoverruns = queueNumberOverruns(ProcessCmdPktQ);

    if (syncpkts != rsyncpkts)
    {
        syncpkts = rsyncpkts;
    }

    if (cmdpkts != rcmdpkts)
    {
        cmdpkts = rcmdpkts;
    }

    if (syncoverruns != rsyncoverruns)
    {
        syncoverruns = rsyncoverruns;
        dbgMessagef("Sync Q Overruns: %d",syncoverruns);
    }

    if (cmdoverruns != rcmdoverruns)
    {
        cmdoverruns = rcmdoverruns;
        dbgMessagef("Cmd Q Overruns: %d",cmdoverruns);
    }
}

void CaptaincyChangedNotify(void)
{
    KeepAliveStartTimers();     // restart keepalive timers after captaincy changes
}

/*=============================================================================
    Keep Alive functions
=============================================================================*/

void UpdateDroppedOutStatuses(void);
void SendKeepAlivePacket(void);
void BroadcastAliveStatusPacket(void);

void KeepAliveReset(void)
{
    sdword i;

    TTimerInit(&SendIAmAliveTimer);
    TTimerInit(&SendAliveStatusTimer);

    LockMutex(AliveTimeoutStatusesMutex);
    for (i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
    {
        TTimerInit(&AliveTimeoutTimers[i]);
        AliveStatuses[i] = ALIVESTATUS_ALIVE;
        HaveKilledPlayerDueToDropout[i] = FALSE;
    }
    UnLockMutex(AliveTimeoutStatusesMutex);

    KeepAliveCalledFirstTime = FALSE;
}

void KeepAliveStartup(void)
{
    AliveTimeoutStatusesMutex = gameCreateMutex();
    KeepAliveReset();
}

void KeepAliveShutdown(void)
{
    gameCloseMutex(AliveTimeoutStatusesMutex);
}

void KeepAliveStartTimers(void)
{
    sdword i;

    TTimerStart(&SendIAmAliveTimer,KEEPALIVE_SEND_IAMALIVE_TIME);
    TTimerStart(&SendAliveStatusTimer,KEEPALIVE_SEND_ALIVESTATUS_TIME);

    LockMutex(AliveTimeoutStatusesMutex);
    for (i=0;i<sigsNumPlayers;i++)
    {
        TTimerStart(&AliveTimeoutTimers[i],KEEPALIVE_IAMALIVE_TIMEOUT);
    }
    UnLockMutex(AliveTimeoutStatusesMutex);
}

void KillAnyDroppedOutPlayers(void)
{
    sdword i,j;
    udword killmask = 0;

    for (i=0;i<sigsNumPlayers;i++)
    {
        if (playerHasDroppedOutOrQuit(i) && (!HaveKilledPlayerDueToDropout[i]))
        {
            HaveKilledPlayerDueToDropout[i] = TRUE;
            killmask |= (1<<i);

            // are there any computer players on his machine we should kill too?

            for (j=sigsNumPlayers;j<universe.numPlayers;j++)
            {
                if (ComputerPlayerOn[j] == i)
                {
                    killmask |= (1<<j);
                }
            }
        }
    }

    if (!killmask)
    {
        return;
    }

    clSendPlayerDropped(killmask);
}

// CAT-TODO(stage 5 LAN): KeepAliveDropPlayerCB 0x00451e70 stamps printTimeout with universe.realtimeelapsed (HW1 totaltimeelapsed)
void KeepAliveDropPlayerCB(sdword playerindex)
{
    LockMutex(AliveTimeoutStatusesMutex);
    if (AliveStatuses[playerindex] != ALIVESTATUS_DROPPEDOUT)
    {
        playersDropped[numPlayerDropped] = playerindex;
        numPlayerDropped++;
        printTimeout = universe.totaltimeelapsed;
        captaincyLog(FALSE,"Player %d was dropped",playerindex);
    }
    AliveStatuses[playerindex] = ALIVESTATUS_DROPPEDOUT;
    UnLockMutex(AliveTimeoutStatusesMutex);
}

void KeepAliveDontDropPlayerCB(sdword playerindex)
{
    LockMutex(AliveTimeoutStatusesMutex);
    TTimerReset(&AliveTimeoutTimers[playerindex]);
    AliveStatuses[playerindex] = ALIVESTATUS_ALIVE;
    UnLockMutex(AliveTimeoutStatusesMutex);
}

void KeepAliveUpdate(void)
{
    if (CaptainNotDefined)
    {
        return;
    }

    if (playPackets)
    {
        return;
    }

    if (!KeepAliveCalledFirstTime)
    {
        KeepAliveStartTimers();
        KeepAliveCalledFirstTime = TRUE;
    }

    if (!IAmCaptain)
    {
        if (TTimerUpdate(&SendIAmAliveTimer))
        {
            TTimerReset(&SendIAmAliveTimer);
            SendKeepAlivePacket();
        }

        UpdateDroppedOutStatuses();
    }
    else
    {
        sdword i;

        LockMutex(AliveTimeoutStatusesMutex);
        for (i=0;i<sigsNumPlayers;i++)
        {
            if (i == sigsPlayerIndex)
            {
                AliveStatuses[i] = ALIVESTATUS_ALIVE;     // I am alive if I'm here
                continue;
            }

            if (playersReadyToGo[i] == PLAYER_QUIT)
            {
                continue;
            }

            if (TTimerUpdate(&AliveTimeoutTimers[i]))
            {
                // timed out, set to
                if (AliveStatuses[i] == ALIVESTATUS_ALIVE)
                {
                    if (universe.players[i].playerState == PLAYER_DEAD)
                    {
                        // player already dead, no need to display drop out box
                        AliveStatuses[i] = ALIVESTATUS_DROPPEDOUT;
                    }
                    else
                    {
                        if ((utyPlayerDroppedDisplay == -1) && (!(smZoomingIn|smZoomingOut)))
                        {
                            utyStartDroppedDialog(i);
                            AliveStatuses[i] = ALIVESTATUS_QUERYINGDROPOUT;
                        }
                        else
                        {
                            AliveStatuses[i] = ALIVESTATUS_WAITQUERYDROPOUTLATER;
                        }
                    }
                }
            }
        }

        if ((utyPlayerDroppedDisplay == -1) && (!(smZoomingIn|smZoomingOut)))
        {
            for (i=0;i<sigsNumPlayers;i++)
            {
                if (i == sigsPlayerIndex)
                {
                    continue;
                }

                if (playersReadyToGo[i] == PLAYER_QUIT)
                {
                    continue;
                }

                if (AliveStatuses[i] == ALIVESTATUS_WAITQUERYDROPOUTLATER)
                {
                    if (universe.players[i].playerState == PLAYER_DEAD)
                    {
                        // player already dead, no need to display drop out box
                        AliveStatuses[i] = ALIVESTATUS_DROPPEDOUT;
                    }
                    else
                    {
                        utyStartDroppedDialog(i);
                        AliveStatuses[i] = ALIVESTATUS_QUERYINGDROPOUT;
                        break;
                    }
                }
            }
        }

        UnLockMutex(AliveTimeoutStatusesMutex);

        if (TTimerUpdate(&SendAliveStatusTimer))
        {
            TTimerReset(&SendAliveStatusTimer);
            BroadcastAliveStatusPacket();
        }

        UpdateDroppedOutStatuses();
        KillAnyDroppedOutPlayers();
    }
}

void SendKeepAlivePacket(void)
{
    HWPacketHeader packet;

    dbgAssertOrIgnore(!CaptainNotDefined);
    dbgAssertOrIgnore(!IAmCaptain);

    packet.type = PACKETTYPE_IAMALIVE;
    packet.from = (uword)sigsPlayerIndex;
    packet.frame = sigsPlayerIndex;
    packet.numberOfCommands = 0;

    titanSendPointMessage(captainIndex,(ubyte *)&packet,sizeof(HWPacketHeader));
}

void BroadcastAliveStatusPacket(void)
{
    AliveStatusPacket packet;

    dbgAssertOrIgnore(!CaptainNotDefined);
    dbgAssertOrIgnore(IAmCaptain);

    packet.packetheader.type = PACKETTYPE_ALIVESTATUS;
    packet.packetheader.from = (uword)sigsPlayerIndex;
    packet.packetheader.frame = sigsPlayerIndex;
    packet.packetheader.numberOfCommands = 0;

    memcpy(packet.alivestatus,AliveStatuses,sizeof(AliveStatuses));

    titanSendBroadcastMessage((ubyte *)&packet,sizeof(packet));     // send to everyone else
}

void UpdateDroppedOutStatuses(void)
{
    sdword i;

    if (playPackets)
    {
        return;
    }

    for (i=0;i<sigsNumPlayers;i++)
    {
        if (AliveStatuses[i] == ALIVESTATUS_DROPPEDOUT)
        {
            if (playersReadyToGo[i] != PLAYER_QUIT)
            {
                playersReadyToGo[i] = PLAYER_DROPPED_OUT;
            }
        }
        else if (AliveStatuses[i] == ALIVESTATUS_ALIVE)
        {
            if (playersReadyToGo[i] != PLAYER_QUIT)
            {
                playersReadyToGo[i] = TRUE;
            }
        }
    }
}

void receivedAliveStatusPacket(AliveStatusPacket *packet,udword sizeofPacket)
{
    dbgAssertOrIgnore(packet->packetheader.type == PACKETTYPE_ALIVESTATUS);

    LockMutex(AliveTimeoutStatusesMutex);
    memcpy(AliveStatuses,packet->alivestatus,sizeof(AliveStatuses));

    UnLockMutex(AliveTimeoutStatusesMutex);
}

void receivedIAmAlivePacket(HWPacketHeader *packet,udword sizeofPacket)
{
    uword aliveplayer;

    dbgAssertOrIgnore(packet->type == PACKETTYPE_IAMALIVE);

    aliveplayer = packet->from;

    if (aliveplayer < sigsNumPlayers)
    {
        LockMutex(AliveTimeoutStatusesMutex);
        if (AliveStatuses[aliveplayer] == ALIVESTATUS_ALIVE)        // keep alive alive players
        {
            TTimerReset(&AliveTimeoutTimers[aliveplayer]);
        }
        UnLockMutex(AliveTimeoutStatusesMutex);
    }
}

void clPlayerDropped(udword playerMask,udword verify)
{
    sdword i;
    bool32 aPlayerDied = FALSE;

    if ((playerMask ^ verify) != KILLDROPPEDOUTPLAYER_VERIFY)
    {
        return;
    }

    for (i=0;i<universe.numPlayers;i++)
    {
        if (playerMask & (1<<i))
        {
            if (universe.players[i].playerState != PLAYER_DEAD)
            {
                univKillPlayer(i,PLAYERKILLED_DROPPEDOUT,-1);   // CAT: nobody credited (clProcessSyncPacket 0x00450e47 pushes -1, -1, i)
                aPlayerDied = TRUE;
            }
        }
    }

    if (aPlayerDied)
    {
        CheckPlayerWin();
    }
}

