// =============================================================================
//  Battle.c
//  - code to evaluate and update battle chatter
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 12/14/1998 by lmoloney
// =============================================================================

#include "Battle.h"

#include "Alliance.h"
#include "CommandDefs.h"
#include "FastMath.h"
#include "Gun.h"
#include "mainrgn.h"
#include "Ping.h"
#include "Randy.h"
#include "Select.h"
#include "SoundEvent.h"
#include "StatScript.h"
#include "Universe.h"
#include "SinglePlayer.h"      // CAT: singlePlayerGameInfo (mission 13 has no battle assessment)
#include <stdio.h>    // HWI-FIX(release-1.0): declared on every platform (mingw does not pull it in)
extern real32 nearestShipDistance;  // CAT: SoundEvent.c (battleShipDyingWithTimeToScream)

/*=============================================================================
    Data:
=============================================================================*/
#if BATTLE_TEXT
char *battleString_WingmanChased[] =
{
    "There's one on your tail. Check six. Check six.",
    "You're being followed.",
    "You've got one closing behind.",
    "You've gotta tail.",
    "You've picked one up.",
    "There's one on you.",
    "Break! Break!",
    NULL
};
char *battleString_WingmanChased_Rsp[] =
{
    "He's on tight.",
    "...Can't break it...",
    "...I see 'em. Cover me.",
    "Roger that trailer, I see him.",
    "Get him off me!",
    "Gotcha.",
    NULL
};
char *battleString_WingmanHit[] =
{
    "Wing hit!",
    "...Cover me dammit.",
    "I've been hit.",
    "I'm hit, but not too bad.",
    "Okay, I'm still with you.",
    "(to self) Damn! Okay, alright. I'm alright...",
    NULL
};
char *battleString_WingmanHit_Rsp[] =
{
    "Alright, hang in there.",
    "Looks OK.",
    "Hold formation. Let's keep it together now...",
    "You're looking fine.",
    "Hang on ... I'm coming in.",
    "On my way.",
    NULL
};
char *battleString_WingmanLethal[] =
{
    "Can't stabilize... I'm... <static>",
    "I'm losing the... <static>",
    "(to self) (a big scream cut short)... <static>",
    "(to self) (breathes in hard and fast)... <static>",
    "aaargh (effort) ... <static>",
    "ahhhhhhhh (burning) ... <static>",
    "hunngh! (depressurization) .. <static>",
    "I'm folding! ...<static>",
    "Mayday, I got no...<static>",
    NULL
};
char *battleString_WingmanDies[] =
{
    "Wingman down",
    "I've lost my wingman",
    "Wing down.",
    "Lost the wing.",
    NULL
};
char *battleString_LeaderChased[] =
{
    "Group leader, you've got a tail.",
    "Group lead, you've picked up a trailer.",
    "Hold on group leader, I'm coming.",
    NULL
};
char *battleString_Wingman[] =
{
    "Wing, watch your course.",
    "You're sliding off-line. Correct.",
    "You're off target vector... compensate now.",
    "Stay within targeting threshold.",
    "Counter that angle, wing.",
    "We're losing wing lock... reset.",
    "You're too close, watch the gap.",
    "Close the gap your too far!",
    "Wingman, tighten it up.",
    "Stay on target.",
    NULL
};
char *battleString_PositiveBattle[] =
{
    "Nice move.",
    "I saw it, yeah, very smooth.",
    "OK, looking fine over here.",
    "Cover me.",
    "I got you.",
    "I'm with you.",
    "That's affirmative, I have it here.",
    "We're on it.",
    "Confirmed visual. Locked.",
    "Target assignments confirmed.",
    "Formation secured.",
    "Coordinates entered.",
    "Got it.",
    "Once again, the way it's done.",
    "Boom. Right there.",
    "Oh yeah.",
    "We're in. Yes.",
    NULL
};
char *battleString_NegativeBattle[] =
{
    "Redirect fire! Redirect fire!",
    "What's happening?",
    "I can't see! Scopes are dead!",
    "Negative! Left! Go left!",
    "Say again?",
    "Behind you!",
    "What's the position?",
    "Can't lock in!",
    "Alarms! I got alarms!",
    "Sound off!",
    "Dammit!",
    "Fall back... ",
    "Respond! ",
    "Talk to me...",
    "Hold on...",
    "What are you doing?",
    "Come on!",
    "Let's move!",
    "Go around!",
    "No! Negative!",
    "Regroup! Regroup!",
    "This ain't happening, man.",
    NULL
};
char *battleString_NeutralBattle[] =
{
    "Tactical even. It's flat.",
    "We're this far, push it home!",
    "Lay it on! Over the top!",
    "Lead your targets...",
    "Collision avoidance... lock it up.",
    "It's pitched. Nudge it! Nudge it our way!",
    "Battle stats level.",
    "Who has it?",
    "Watch the targets, not tactical.",
    "Pick them up fast.",
    "Plug in! Plug in!",
    "We're holding positions...",
    "Flanking ships stay close.",
    NULL
};
char *battleString_FriendlyFire[] =
{
    "Check your fire!  Check your fire!",
    "Check your targets!  Friendly fire!",
    "Watch it!",
    "Watch that friendly fire!",
    NULL
};
char *battleString_Kamikaze[] =
{

    "[increasing pitch] ...mmmmmmmnnahh!!!",
    "....nnnnn....aaahh!",
    "..aaaarrrgh!",
    "FOR THE HOMEWORLD!!!!",
    "...uh....understood...  (hesitant)",
    "EAT THIS!!!",

    NULL
};
char *battleString_StartBattleDisadvantaged[] =
{

    "We're outnumbered.",
    "I have a bad feeling about this...",
    "This isn't lookin' good Fleet...",
    "(to self quietly) ..ohh man...",

    NULL
};
char *battleString_StartBattleDisadvantaged_Rsp[] =
{
    "Yeah, but we can do it.",
    "Just keep it tight.",
    "We've got our orders.",
    "Shut it, we're goin' in.",
    NULL
};
char *battleString_StartBattleFair[] =
{

    "Watch your six.",
    "Pick your targets.",
    "It looks even, let's do it.",

    NULL
};
char *battleString_StartBattleFair_Rsp[] =
{

    "Maintain positions.",
    "Right behind you.",
    "Acknowledged. On your wing.",
    "Coming in at <NumberList>.",
    "Look for the trap.",

    NULL
};
char *battleString_StartBattleAdvantaged[] =
{

    "We caught 'em napping.",
    "I almost feel bad about this.",
    "It's in our favor.",
    "Keep one eye on the clock.",
    "Looks like we got the upper hand.",
    "Let's make this quick and painless.",

    NULL
};
char *battleString_StartBattleAdvantaged_Rsp[] =
{

    "Stay on top of them.",
    "Don't let them regroup.",
    "It's not over yet.",
    "Get on their tails, and stay there.",
    "Copy that ... let's take 'em down one by one.",
    "They're up against the wall.",

    NULL
};
char *battleString_Retreat[] =
{

    "All units retreat, repeat, all units retreat.",
    "Get out of there now! Abort. Abort.",
    "Abort mission, repeat, abort mission.",
    "Retreat. Retreat. Abort all offensive action.",
    "Begin abort sequence.",
    "Abort code <NumberList>.",
    "Break! Break!",
    "Disengage immediately.",

    NULL
};
char *battleString_Retreat_Rsp[] =
{

    "<Roger>. On our way.",
    "<Roger>. We're outta here!",
    "<Roger>. It's about time!",
    "Abort code received and confirmed.",
    "<Roger>. Disengaging.",
    "Let's get the hell outta Dodge!",
    "Wing leaders... head count, report all losses.",

    NULL
};
char *battleString_WinningBattle[] =
{

    "We're on top of it.",
    "This is looking good. Stay tight.",
    "It's ours so far. Watch the counter.",
    "Spreading at <Degrees>. Good work.",
    "Just a little more and it's over.",
    "Tactical full, looks good.",
    "Let's wrap this up.",

    NULL
};
char *battleString_WinningBattle_Rsp[] =
{

    "Yup.",
    "Roger that sweep.",
    "Holding attack pattern <Letter>.",
    "That's it. There we go.",
    "They're going down.",

    NULL
};
char *battleString_KickingButt[] =
{

    "Let's end this.",
    "It's over. Let's clean it up.",
    "We have them.",
    "This is finished.",
    "Pick off the stragglers.",
    "OK that'll do it.",
    "The enemy is failing.",

    NULL
};
char *battleString_KickingButt_Rsp[] =
{
    "Roger that. Cleaning it up.",
    "Affirmative. It'll be no problem.",
    "No contest.",
    "They're almost down.",
    "Watch for debris.",
    "Reset targeting and inertials.",
    "Let's do them a favour.",
    "They're routed.",
    NULL
};
char *battleString_BattleWon[] =
{

    "The enemy is defeated.",
    "We've won. We've done it.",
    "Stand down from combat stations.",
    "The area is secure.",
    "Mission accomplished. Standing by.",
    "Enemy neutralized, awaiting further instructions.",
    "OK. That's it.  Nice work.",

    NULL
};
char *battleString_LosingBattle[] =
{
    "We have a situation developing here.",
    "Requesting back up... repeat... requesting back up.",
    "Switch to battle code <NumberList>.",
    "The enemy has it.",
    "We're taking heavy losses.",
    "We are being overwhelmed.",
    "We're losing grip here.",
    "Head count, head count! Report in!",
    "Links failing, we have losses.",
    "Tactical is thinning out here, we've got a problem.",
    "... ah ... this is not looking good Fleet.",
    NULL
};
char *battleString_LosingBattle_Rsp[] =
{

    "OK, keep it together now.",
    "Return the fight to them.",
    "Roger that contact, we see it.",
    "Hang in there. Hold your positions.",
    "Losses confirmed.",
    "Get it together and fight back.",
    "Assume defense lock <Letter>.",
    "Open up gaps. Look for the gaps.",
    "<Roger>. Stay close. Keep it close.",

    NULL
};
char *battleString_LosingBadly[] =
{

    "Situation critical Fleet.",
    "We're getting our asses kicked here Fleet!",
    "We're down to <Number> units.",
    "We're pinned. We need relief.",
    "Hits! We got hits all over here!",
    "Contact everywhere! Heavy contact!",
    "Is anyone responding here?",
    "Revise head counts! Updates!",
    "Where are the links?",
    "Tactical is almost clear. It's getting a bit lonely out here...",
    "Now it's really not looking good Fleet...",
    "Check your fire! Check your fire!",

    NULL
};
char *battleString_LosingBadly_Rsp[] =
{
    "Emergency procedures.",
    "Rally! Rally!",
    "Get us out of here!",
    "Maintain positions!",
    "We're running out of time.",
    "Can't ... shake 'em... Can't...",
    "It's too much! I can't.... damn!",
    "Come ON!  Let's GO!",
    "If you're gonna act, act now.",
    "This is murder out here.",
    NULL
};
#endif //BATTLE_TEXT

//master list of chatter events
// CAT: Cataclysm's 88 battle chatter events (table .data 0x0074e8d0, stride 0x58, read by
//      batRandomWeightSetCB 0x004232c0 and the other bat*SetCB callbacks). Same 88 slots and BCE_* order as HW1; the
//      names are Cataclysm's (BattleChatter.script uses them) and the event numbers are the exe's speech event ids
//      (Cataclysm format: index in the low bits, actor flags from 0x800, type flags from 0x1000000). Slots 14, 45, 66,
//      73, 75, 83 and 84 carry new events; 76, 77, 81 and 87 have event number 0 in the exe.
battleevent battleChatterEvent[] =
{
/*BCE_Wingman,                  */ newEvent("CHAT_Fighter_Wingman", 0x10008c4, battleString_Wingman),
/*BCE_PositiveBattle            */ newEvent("CHAT_Intel_Grp_PositiveBattle", 0x100207f, battleString_PositiveBattle),  // HW1: CHAT_Grp_PositiveBattle
/*BCE_PositiveBattle_More       */ newEvent("CHAT_Ship_Grp_PositiveBattleMore", 0x1000816, battleString_PositiveBattle),  // HW1: CHAT_Grp_PositiveBattle_More
/*BCE_NegativeBattle            */ newEvent("CHAT_Intel_Grp_NegativeBattle", 0x1002080, battleString_NegativeBattle),  // HW1: CHAT_Grp_NegativeBattle
/*BCE_NegativeBattle_More       */ newEvent("CHAT_Ship_Grp_NegativeBattleMore", 0x1000817, battleString_NegativeBattle),  // HW1: CHAT_Grp_NegativeBattle_More
/*BCE_NegativeBattle_More2      */ newEvent("CHAT_Intel_Grp_NegativeBattleMore2", 0x100206f, battleString_NegativeBattle),  // HW1: CHAT_Grp_NegativeBattle_More2
/*BCE_NeutralBattle,            */ newEvent("CHAT_Intel_Grp_NeutralBattle", 0x1002081, battleString_NeutralBattle),  // HW1: CHAT_Grp_NeutralBattle
/*BCE_NeutralBattle_More,       */ newEvent("CHAT_Intel_Grp_NeutralBattleMore", 0x1002071, battleString_NeutralBattle),  // HW1: CHAT_Grp_NeutralBattle_More
/*BCE_FriendlyFire,             */ newEvent("CHAT_Intel_Grp_FriendlyFire", 0x1002082, battleString_FriendlyFire),  // HW1: CHAT_Grp_FriendlyFire
/*BCE_GroupFriendlyFire,        */ newEvent("STAT_MoShip_AssGrp_FriendlyFire", 0x200105f, battleString_FriendlyFire),  // HW1: STAT_AssGrp_FriendlyFire
/*BCE_WingmanChased,            */ newEvent("STAT_Fighter_WingmanChased", 0x20008be, battleString_WingmanChased),
/*BCE_WingmanChasedR            */ newEvent("STAT_Fighter_WingmanChasedRsp", 0x20008bf, battleString_WingmanChased_Rsp),  // HW1: STAT_Fighter_WingmanChased_Rsp
/*BCE_WingmanHit,               */ newEvent("STAT_Fighter_WingmanHit", 0x20008c0, battleString_WingmanHit),
/*BCE_WingmanHitRsp             */ newEvent("STAT_Fighter_WingmanHitRsp", 0x20008c1, battleString_WingmanHit_Rsp),  // HW1: STAT_Fighter_WingmanHit_Rsp
/*BCE_WingmanLethal,            */ newEvent("STAT_Spec_ShipBeingBeastified", 0x10008a1, battleString_WingmanLethal),  // HW1: STAT_Fighter_WingmanLethal
/*BCE_WingmanDies,              */ newEvent("STAT_Fighter_WingmanDies", 0x20008c2, battleString_WingmanDies),
/*BCE_LeaderChased,             */ newEvent("STAT_Fighter_LeaderChased", 0x20008c3, battleString_LeaderChased),
/*BCE_Kamikaze,                 */ newEvent("COMM_LInt_Kamikaze", 0x40008c5, battleString_Kamikaze),
/*BCE_StartBattleDisadvant,     */ newEvent("STAT_Intel_Grp_StartBattleDisadvant", 0x2002077, battleString_StartBattleDisadvantaged),  // HW1: STAT_Grp_StartBattleDisadvant
/*BCE_StartBattleDisadvantRsp,  */ newEvent("STAT_Intel_Grp_StartBattleDisadvantRsp", 0x200206c, battleString_StartBattleDisadvantaged_Rsp),  // HW1: STAT_Grp_StartBattleDisadvant_Rsp
/*BCE_StartBattleFair,          */ newEvent("STAT_Intel_Grp_StartBattleFair", 0x2002078, battleString_StartBattleFair),  // HW1: STAT_Grp_StartBattleFair
/*BCE_StartBattleFairRsp,       */ newEvent("STAT_Intel_Grp_StartBattleFairRsp", 0x200206d, battleString_StartBattleFair_Rsp),  // HW1: STAT_Grp_StartBattleFair_Rsp
/*BCE_StartBattleAdvantaged,    */ newEvent("STAT_Intel_Grp_StartBattleAdvantaged", 0x2002079, battleString_StartBattleAdvantaged),  // HW1: STAT_Grp_StartBattleAdvantaged
/*BCE_StartBattleAdvantagedRsp, */ newEvent("STAT_Intel_Grp_StartBattleAdvantagedRsp", 0x200206e, battleString_StartBattleAdvantaged_Rsp),  // HW1: STAT_Grp_StartBattleAdvantaged_Rsp
/*BCE_Retreat,                  */ newEvent("COMM_Intel_Grp_Retreat", 0x4002072, battleString_Retreat),  // HW1: COMM_Group_Retreat
/*BCE_RetreatRsp,               */ newEvent("COMM_Intel_Grp_RetreatRsp", 0x400206b, battleString_Retreat_Rsp),  // HW1: COMM_Group_Retreat_Rsp
/*BCE_WinningBattle,            */ newEvent("STAT_Intel_Grp_WinningBattle", 0x200207a, battleString_WinningBattle),  // HW1: STAT_Grp_WinningBattle
/*BCE_WinningBattleRsp,         */ newEvent("STAT_Ship_Grp_WinningBattleRsp", 0x1000812, battleString_WinningBattle_Rsp),  // HW1: STAT_Grp_WinningBattle_Rsp
/*BCE_KickingButt,              */ newEvent("STAT_Intel_Grp_KickingButt", 0x200207b, battleString_KickingButt),  // HW1: STAT_Grp_KickingButt
/*BCE_KickingButtRsp,           */ newEvent("STAT_Ship_Grp_KickingButtRsp", 0x1000813, battleString_KickingButt_Rsp),  // HW1: STAT_Grp_KickingButt_Rsp
/*BCE_BattleWon,                */ newEvent("STAT_Intel_Grp_BattleWon", 0x200207c, battleString_BattleWon),  // HW1: STAT_Grp_BattleWon
/*BCE_GroupVictory,             */ newEvent("STAT_Intel_AssGrp_Victory", 0x2002075, battleString_BattleWon),  // HW1: STAT_F_AssGrp_Victory
/*BCE_LosingBattle,             */ newEvent("STAT_Intel_Grp_LosingBattle", 0x200207d, battleString_LosingBattle),  // HW1: STAT_Grp_LosingBattle
/*BCE_LosingBattle_More,        */ newEvent("STAT_Intel_Grp_LosingBattleMore", 0x2002070, battleString_LosingBattle),  // HW1: STAT_Grp_LosingBattle_More
/*BCE_LosingBattleRsp,          */ newEvent("STAT_Ship_Grp_LosingBattleRsp", 0x1000814, battleString_LosingBattle_Rsp),  // HW1: STAT_Grp_LosingBattle_Rsp
/*BCE_LosingBadly,              */ newEvent("STAT_Intel_Grp_LosingBadly", 0x200207e, battleString_LosingBadly),  // HW1: STAT_Grp_LosingBadly
/*BCE_LosingBadly_More,         */ newEvent("STAT_Ship_Grp_LosingBadlyMore", 0x1000819, battleString_LosingBadly),  // HW1: STAT_Grp_LosingBadly_More
/*BCE_LosingBadlyRsp,           */ newEvent("STAT_Ship_Grp_LosingBadlyRsp", 0x1000815, battleString_LosingBadly_Rsp),  // HW1: STAT_Grp_LosingBadly_Rsp
/*COMM_CannotComply,            */ newEvent("COMM_CannotComply", 0x4000810, NULL),
/*STAT_Strike_StuckInGrav,      */ newEvent("STAT_MoShip_GravWellHasShips", 0x2001051, NULL),  // HW1: STAT_Strike_StuckInGrav
/*COMM_Grav_Off,                */ newEvent("COMM_Spec_GravWellOff", 0x40008a0, NULL),  // HW1: COMM_Grav_Off
/*STAT_Strike_AttackComplete,   */ newEvent("STAT_MoShip_StrikeAttackComplete", 0x200105a, NULL),  // HW1: STAT_Strike_AttackComplete
/*COMM_Kamikaze_NoTargets,      */ newEvent("COMM_Kamikaze_NoTargets", 0x4000809, NULL),
/*COMM_HVette_BurstAttack,      */ newEvent("COMM_Spec_FighterBurstAttack", 0x400089a, NULL),  // HW1: COMM_HVette_BurstAttack
/*COMM_MLVette_ForceDrop,       */ newEvent("COMM_Spec_MineLayerForceDrop", 0x400089b, NULL),  // HW1: COMM_MLVette_ForceDrop
/*COMM_MissleDest_VolleyAttack, */ newEvent("STAT_Spec_HiveSwarmersUnderway", 0x20008ab, NULL),  // HW1: COMM_MissleDest_VolleyAttack
/*STAT_Grp_RetreatSuccessful,   */ newEvent("STAT_Intel_Grp_RetreatSuccessful", 0x2002083, NULL),  // HW1: STAT_Grp_RetreatSuccessful
/*STAT_AssGrp_RetreatSuccessful,*/ newEvent("STAT_MoShip_AssGrp_RetreatSuccessful", 0x200105d, NULL),  // HW1: STAT_AssGrp_RetreatSuccessful
/*STAT_Grp_EnemyRetreated,      */ newEvent("STAT_Intel_Grp_EnemyRetreated", 0x2002084, NULL),  // HW1: STAT_Grp_EnemyRetreated
/*STAT_AssGrp_EnemyRetreated,   */ newEvent("STAT_MoShip_AssGrp_EnemyRetreated", 0x200105e, NULL),  // HW1: STAT_AssGrp_EnemyRetreated
/*STAT_SCVette_DroppingOff,     */ newEvent("STAT_Spec_SalvageDropOff", 0x2000897, NULL),  // HW1: STAT_SCVette_DroppingOff
/*STAT_F_ShipStolen,            */ newEvent("STAT_MoShip_ShipStolen", 0x2001041, NULL),  // HW1: STAT_F_ShipStolen
/*STAT_Grp_InMineField,         */ newEvent("STAT_Intel_Grp_InMineField", 0x2002087, NULL),  // HW1: STAT_Grp_InMineField
/*STAT_AssGrp_InMineField,      */ newEvent("STAT_MoShip_AssGrp_InMineField", 0x2001060, NULL),  // HW1: STAT_AssGrp_InMineField
/*BCE_STAT_F_CloakedShipsDete   */ newEvent("STAT_MoShip_CloakedShipsDetected", 0x200104c, NULL),  // HW1: STAT_F_CloakedShipsDetected
/*BCE_STAT_Grp_AttackRetaliat   */ newEvent("STAT_Intel_Grp_AttackRetaliate", 0x2002086, NULL),  // HW1: STAT_Grp_AttackRetaliate
/*BCE_STAT_Grp_AttackRetaliat,  */ newEvent("STAT_Ship_Grp_AttackRetaliateRepeat", 0x1000818, NULL),  // HW1: STAT_Grp_AttackRetaliate_Repeat
/*BCE_STAT_AssGrp_AttackRetal   */ newEvent("STAT_Intel_AssGrp_AttackRetaliate", 0x2002073, NULL),  // HW1: STAT_AssGrp_AttackRetaliate
/*BCE_STAT_AssGrp_AttackRetaleat*/ newEvent("STAT_Intel_AssGrp_AttackRetaliateRepeat", 0x2002074, NULL),  // HW1: STAT_AssGrp_AttackRetaliate_Repeat
/* BCE_AttackFromAlly,          */ newEvent("STAT_MoShip_UnderAttackFromAlly", 0x2001069, NULL),  // HW1: STAT_F_UnderAttackFromAlly
/* BCE_GroupAttackFromAlly,     */ newEvent("STAT_Intel_Grp_AttackFromAlly", 0x2002085, NULL),  // HW1: STAT_Grp_AttackFromAlly
/* BCE_ProbeDetected,           */ newEvent("STAT_MoShip_EnemyProbeDetected", 0x200104b, NULL),  // HW1: STAT_F_EnemyProbe_Detected
/* BCE_GravwellDetected,        */ newEvent("STAT_MoShip_GravWellDetected", 0x2001050, NULL),  // HW1: STAT_F_GravWellDetected
/* BCE_CapitalShipDamaged,      */ newEvent("STAT_Ship_Damaged", 0x100080b, NULL),  // HW1: STAT_Cap_Damaged
/* BCE_CapitalShipDies,         */ newEvent("STAT_Cap_Dies", 0x100080c, NULL),
/* BCE_CapitalShipDiesReport,   */ newEvent("STAT_MoShip_ShipDiesReport", 0x2001065, NULL),  // HW1: STAT_Cap_Dies_Report
/* BCE_HyperspaceAbandoned      */ newEvent("STAT_Spec_RammerRamComplete", 0x20008ae, NULL),  // HW1: STAT_Hyper_Abandoned
/* BCE_MothershipLargeAttack,   */ newEvent("STAT_MoShip_MoShipLargeAttack", 0x2001059, NULL),  // HW1: STAT_MoShip_LargeAttack
/* BCE_MothershipLargeAttackResp*/ newEvent("STAT_MoShip_MoShipUnderLargeAttackRsp", 0x2001057, NULL),  // HW1: STAT_MoShip_LargeAttack_Resp
/* BCE_CapitalShipLaunched,     */ newEvent("STAT_MoShip_CapShipLaunched", 0x200103f, NULL),  // HW1: STAT_Cap_Launched
/* BCE_CapitalShipWelcomed,     */ newEvent("STAT_MoShip_CapShipWelcomed", 0x200101e, NULL),  // HW1: STAT_Cap_Welcomed
/* BCE_CapitalDamagedSpecific,  */ newEvent("STAT_MoShip_DamagedSpecificShip", 0x2001066, NULL),  // HW1: STAT_Cap_Damaged_Specific
/* BCE_CapitalShipDocked,       */ newEvent("STAT_Cap_ShipDocked", (sdword)0x81000804, NULL),
/* BCE_DockingChatter,          */ newEvent("STAT_MoShip_EMPPulseDetected", 0x200104f, NULL),  // HW1: CHAT_Docking
/* BCE_DockingChatterMore,      */ newEvent("CHAT_Docking_More", (sdword)0x81000804, NULL),
/* BCE_CloakingOn,              */ newEvent("STAT_Spec_WorkerFull", 0x20008a4, NULL),  // HW1: COMM_Cloak_CloakingOn
/* BCE_CloakingOnResponse,      */ newEvent("STAT_Cloak_CloakingOn_Resp", 0x0, NULL),
/* BCE_CloakingInsufficientPower*/ newEvent("COMM_Cloak_InsufficientPower", 0x0, NULL),
/* BCE_CloakingPowerLow,        */ newEvent("STAT_Spec_CloakCloakPowerLow", 0x200088c, NULL),  // HW1: STAT_Cloak_CloakPowerLow
/* BCE_Decloaking,              */ newEvent("COMM_Spec_CloakDecloak", 0x400088d, NULL),  // HW1: COMM_Cloak_Decloak
/* BCE_SalvageTargetAcquired,   */ newEvent("STAT_Spec_SalvageTargetAcquired", 0x2000899, NULL),  // HW1: STAT_SCVette_TargetAcquired
/* BCE_RepairStarted,           */ newEvent("STAT_RepVette_StartedRepairs", 0x0, NULL),
/* BCE_RepairFinished,          */ newEvent("STAT_Spec_FinishedRepairs", 0x2000893, NULL),  // HW1: STAT_RepVette_FinishedRepairs
/* BCE_FuelLow,                 */ newEvent("STAT_Spec_ResourcesTransferred", 0x200088e, NULL),  // HW1: STAT_Strike_LowOnFuel
/* BCE_OutOfFuel,               */ newEvent("COMM_Spec_RammerStartingRam", 0x40008ad, NULL),  // HW1: STAT_Strike_OutOfFuel
/* BCE_COMM_F_ScuttleReaffirm   */ newEvent("COMM_F_ScuttleReaffirm", 0x400080a, NULL),
/* BCE_COMM_F_HyperspaceDetected*/ newEvent("COMM_MoShip_HyperspaceDetected", 0x400104e, NULL),  // HW1: COMM_F_HyperspaceDetected
/* BCE_STAT_F_ProxSensor_Detection*/ newEvent("STAT_MoShip_ProxSensorDetection", 0x0, NULL),  // HW1: STAT_F_ProxSensor_Detection

/*COMM_F_Grp_Enemy_Fighters_Decl*/ //newEvent("COMM_F_Grp_Enemy_Fighters_Decloaking", COMM_F_Grp_Enemy_Fighters_Decloaking , NULL),
/*COMM_F_Cloakgen_Decloaking,   */ //newEvent("COMM_F_Cloakgen_Decloaking", COMM_F_Cloakgen_Decloaking , NULL),
};

//some chatter is class-specific
#define BCD_LaunchWelcome       0x01
#define BCD_Damaged             0x02
ubyte battleClassDependencies[NUM_CLASSES] =
{
    /* CLASS_Mothership   */ 0,
    /* CLASS_HeavyCruiser */ BCD_Damaged | BCD_LaunchWelcome,
    /* CLASS_Carrier      */ BCD_Damaged | BCD_LaunchWelcome,
    /* CLASS_Destroyer    */ BCD_Damaged | BCD_LaunchWelcome,
    /* CLASS_Frigate      */ BCD_Damaged | BCD_LaunchWelcome,
    /* CLASS_Corvette     */ 0,
    /* CLASS_Fighter      */ 0,
    /* CLASS_Resource     */ BCD_Damaged,
    /* CLASS_NonCombat    */ 0,
    // CAT: 12 classes; the three new ones have no class-specific chatter (battleClassDependencies 0x00750710 =
    //      {0,3,3,3,3,0,0,2,0,0,0,0})
    /* CLASS_MonsterShip  */ 0,
    /* CLASS_Component    */ 0,
    /* CLASS_Micro        */ 0
};

/*-----------------------------------------------------------------------------
    Battle chatter tweaks and script parsing table
-----------------------------------------------------------------------------*/
//number of times to try to find a ship to do the talking
sdword batMouthpieceTries = BAT_MouthpieceTries;

//coefficients for computing the current battle standing number (BSN)
real32 batStandingFriendlyPower =   BAT_StandingFriendlyPower;
real32 batStandingFriendlyValue =   BAT_StandingFriendlyValue;
real32 batStandingEnemyPower    =   BAT_StandingEnemyPower;
real32 batStandingEnemyValue    =   BAT_StandingEnemyValue;
/*
real32 batFriendlyLossPower     =   BAT_FriendlyLossPower;
real32 batFriendlyLossValue     =   BAT_FriendlyLossValue;
real32 batEnemyLossPower        =   BAT_EnemyLossPower;
real32 batEnemyLossValue        =   BAT_EnemyLossValue;
*/
real32 batExpLossDevalue        =   BAT_ExpLossDevalue;

//values of BSN for various statuses (BSN > x ?)
real32 batBSNKickingButt        =   BAT_BSNKickingButt;
real32 batBSNWinning            =   BAT_BSNWinning;
real32 batBSNNeutral            =   BAT_BSNNeutral;
real32 batBSNLosing             =   BAT_BSNLosing;

//coefficients for determining if a ship is really on your tail
real32 batBogeyAspect           =   BAT_BogeyAspect;

//retreat parameters
real32 batRetreatModifier       =   BAT_RetreatModifier;

//global parameter for controlling how much chatter there is
// CAT: a float scale of the weights (.data 0x0075074c = 128.0 until the options or the script set it); no
//      batGlobalFrequencyMin / Max (the exe's script table has neither)
real32 batGlobalFrequencyModifier = (real32)BAT_GlobalFrequencyModifier;

// CAT: the last status event said and when (battleChatterFleetAttempt 0x00423a50: .data 0x008df9f4 and 0x00750750,
//      -30.0 at start and after cat_battleChatterReset)
static battleevent *catBatLastStatusEvent = NULL;
static real32 catBatLastStatusTime = -30.0f;

//battle chatter tuning pointer
#if BATTLE_TUNING
battleevent *batChatterToTune   = NULL;
#endif

//script-parsing table and function prototypes
void batMaxDistanceSetCB(char *directory,char *field,void *dataToFillIn);
void batDistanceExponentSetCB(char *directory,char *field,void *dataToFillIn);
void batRandomWeightSetCB(char *directory,char *field,void *dataToFillIn);
void batMinWavelengthSetCB(char *directory,char *field,void *dataToFillIn);
void batWavelengthExponentSetCB(char *directory,char *field,void *dataToFillIn);
void batMinRepeatProximitySetCB(char *directory,char *field,void *dataToFillIn);
void batRepeatProximityExponentSetCB(char *directory,char *field,void *dataToFillIn);
void batTuneEventSet(char *directory,char *field,void *dataToFillIn);
void batGlobalFrequencyModifierSet(char *directory,char *field,void *dataToFillIn);
void cat_batStatusUpdateSetCB(char *directory,char *field,void *dataToFillIn);
void cat_batStatusSuppressSetCB(char *directory,char *field,void *dataToFillIn);
scriptEntry battleTweaks[] =
{
    {"randomWeight",    batRandomWeightSetCB, NULL},
    {"maxDistance",     batMaxDistanceSetCB, NULL},
    {"expDistance",     batDistanceExponentSetCB, NULL},
    {"minWavelength",   batMinWavelengthSetCB, NULL},
    {"expWavelength",   batWavelengthExponentSetCB, NULL},
    {"minRepeatProximity",batMinRepeatProximitySetCB, NULL},
    {"expRepeatProximity",batRepeatProximityExponentSetCB, NULL},
    // CAT: two new per-event keys (script table .data 0x00750758, 12-byte entries: "battleStatusUpdate" entry
    //      0x007507ac, "battleStatusSuppress" entry 0x007507b8; their callback fields are at 0x007507b0/0x007507bc)
    {"battleStatusUpdate",   cat_batStatusUpdateSetCB, NULL},
    {"battleStatusSuppress", cat_batStatusSuppressSetCB, NULL},
#if BATTLE_TUNING
    {"TuneOnlyThisEvent",batTuneEventSet, NULL},
#endif
    {"batGlobalFrequencyModifier", batGlobalFrequencyModifierSet,NULL},
    makeEntry(batMouthpieceTries , scriptSetSdwordCB),
    makeEntry(batStandingFriendlyPower , scriptSetReal32CB),
    makeEntry(batStandingFriendlyValue , scriptSetReal32CB),
    makeEntry(batStandingEnemyPower    , scriptSetReal32CB),
    makeEntry(batStandingEnemyValue    , scriptSetReal32CB),
    makeEntry(batBSNKickingButt , scriptSetReal32CB),
    makeEntry(batBSNWinning     , scriptSetReal32CB),
    makeEntry(batBSNNeutral     , scriptSetReal32CB),
    makeEntry(batBSNLosing      , scriptSetReal32CB),
    makeEntry(batBogeyAspect    , scriptSetReal32CB),
    makeEntry(batRetreatModifier, scriptSetReal32CB),
    
    END_SCRIPT_ENTRY
};

/*=============================================================================
    Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : battleChatterEventFromName
    Description : Find a battle chatter event by name reference
    Inputs      :
    Outputs     : value - value to read from the string after the name
    Return      :
----------------------------------------------------------------------------*/
battleevent *battleChatterEventFromName(char *name, double *value)
{
    sdword index, nScanned;
    char *string;
    real32 floatValue;

    string = strtok(name, BAT_Delimiters);
    for (index = 0; index < BCE_LastBCE; index++)
    {
        if (!strcmp(name, battleChatterEvent[index].eventName))
        {
            //return(&battleChatterEvent[index]);
            goto foundName;
        }
    }
#if BATTLE_ERROR_CHECKING
    dbgFatalf(DBG_Loc, "Unable to find battle chatter event '%s'.", name);
#endif
foundName:
    string = strtok(NULL, BAT_Delimiters);
    nScanned = sscanf(string, "%f", &floatValue);
    dbgAssertOrIgnore(nScanned == 1);
    *value = (double)floatValue;
    return(&battleChatterEvent[index]);
}

/*-----------------------------------------------------------------------------
    Script-parsing callback functions
    CAT: the exe stores the raw values (batRandomWeightSetCB 0x004232c0: (double)(float)value, no PNG_TaskPeriod
         scaling) and the two distances squared (batMaxDistanceSetCB 0x00423330, batMinRepeatProximitySetCB
         0x00423500: value * value in float)
-----------------------------------------------------------------------------*/
void batRandomWeightSetCB(char *directory,char *field,void *dataToFillIn)
{
    battleevent *chat;
    double value;

    chat = battleChatterEventFromName(field, &value);
    chat->randomWeight = (double)(real32)value;
}
void batMaxDistanceSetCB(char *directory,char *field,void *dataToFillIn)
{
    battleevent *chat;
    double value;

    chat = battleChatterEventFromName(field, &value);
    chat->maxDistance = (double)((real32)value * (real32)value);
}
void batDistanceExponentSetCB(char *directory,char *field,void *dataToFillIn)
{
    battleevent *chat;
    double value;

    chat = battleChatterEventFromName(field, &value);
    dbgAssertOrIgnore(value >= 0.0f);
    chat->expDistance = (double)value;
}
void batMinWavelengthSetCB(char *directory,char *field,void *dataToFillIn)
{
    battleevent *chat;
    double value;

    chat = battleChatterEventFromName(field, &value);
    dbgAssertOrIgnore(value > 0.0f);
    chat->minWavelength = (double)value;
}
void batWavelengthExponentSetCB(char *directory,char *field,void *dataToFillIn)
{
    battleevent *chat;
    double value;

    chat = battleChatterEventFromName(field, &value);
    dbgAssertOrIgnore(value > 0.0f);
    chat->expWavelength = (double)value;
}
void batMinRepeatProximitySetCB(char *directory,char *field,void *dataToFillIn)
{
    battleevent *chat;
    double value;

    chat = battleChatterEventFromName(field, &value);
    chat->minRepeatProximity = (double)((real32)value * (real32)value);
}
void batRepeatProximityExponentSetCB(char *directory,char *field,void *dataToFillIn)
{
    battleevent *chat;
    double value;

    chat = battleChatterEventFromName(field, &value);
    dbgAssertOrIgnore(value > 0.0f);
    chat->expRepeatProximity = (double)value;
}
// CAT: "battleStatusUpdate <event> <value>": flag = value > 0.0 (cat_batStatusUpdateSetCB 0x004235f0)
void cat_batStatusUpdateSetCB(char *directory,char *field,void *dataToFillIn)
{
    battleevent *chat;
    double value;

    chat = battleChatterEventFromName(field, &value);
    chat->statusUpdate = (value > 0.0) ? TRUE : FALSE;
}
// CAT: "battleStatusSuppress <event> <value>": flag = value > 0.0 (cat_batStatusSuppressSetCB 0x00423680)
void cat_batStatusSuppressSetCB(char *directory,char *field,void *dataToFillIn)
{
    battleevent *chat;
    double value;

    chat = battleChatterEventFromName(field, &value);
    chat->statusSuppress = (value > 0.0) ? TRUE : FALSE;
}
#if BATTLE_TUNING
void batTuneEventSet(char *directory,char *field,void *dataToFillIn)
{
    double value;
    batChatterToTune = battleChatterEventFromName(field, &value);
}
#endif
void batGlobalFrequencyModifierSet(char *directory,char *field,void *dataToFillIn)
{
    real32 value;
    sdword nScanned;

    nScanned = sscanf(field, "%f", &value);
    dbgAssertOrIgnore(nScanned == 1);
    batGlobalFrequencyModifier = value;         // CAT: the raw float (batGlobalFrequencyModifierSet 0x00423710)
}

/*-----------------------------------------------------------------------------
    Name        : battleChatterStartup
    Description : Startup the battle chatter module by reading in BattleChatter.script
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void battleChatterStartup(void)
{
    scriptSet(NULL, "BattleChatter.script", battleTweaks);  //scan in all the chatter parameters
}

/*-----------------------------------------------------------------------------
    Name        : battleChatterShutdown
    Description : Shut down the battle chatter module
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void battleChatterShutdown(void)
{
}

/*-----------------------------------------------------------------------------
    Name        : cat_battleChatterReset
    Description : CAT: a new level forgets when each event was last said and the last status event
                  (cat_battleChatterReset 0x00423760, from cat_soundLevelReset)
----------------------------------------------------------------------------*/
void cat_battleChatterReset(void)
{
    sdword index;

    catBatLastStatusTime = -30.0f;
    catBatLastStatusEvent = NULL;
    for (index = 0; index < BCE_LastBCE; index++)
    {
        battleChatterEvent[index].lastTimeSpoken = 0.0f;
    }
}

/*-----------------------------------------------------------------------------
    Name        : battleSpeechText
    Description : Print out some text of what the battle chatter _might_ say,
                    not what it actually says.
    Inputs      : eventNum - what event is being called
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
#if BATTLE_TEXT
void battleSpeechText(sdword eventNum, char **texts)
{
    sdword nTexts, index;

    for (nTexts = 0; texts[nTexts] != NULL; nTexts++)
    {                                                       //count number of text strings
        ;
    }
    index = batRandom() % nTexts;
    dbgMessagef("Battle chatter event %d - '%s'.", eventNum, texts[index]);
}
#endif

/*-----------------------------------------------------------------------------
    Name        : battleChatterAttempt
    Description : Attempt to make a given chatter event chat
                  CAT: the exe's test (battleChatterAttempt 0x00423790): one random draw against the raw weight
                  times the frequency scale, the ship's squared camera distance against the squared maxDistance
                  (HW1's distance curve and its second draw are gone); only the local player's ships speak; the
                  capital-ship death line 0x100080c is queued without the ship.
    Inputs      : linkTo - handle of the previously queued event that this one is to follow
                  event - event to attempt to speak
                  ship - ship that'll do the speaking
    Outputs     :
    Return      : handle of queued event, or ERROR (-1) if none queued
----------------------------------------------------------------------------*/
sdword battleChatterAttempt(sdword linkTo, sdword event, Ship *ship, sdword variable)
{
    battleevent *chat;
    real32 weight;

    dbgAssertOrIgnore(event >= 0 && event < BCE_LastBCE);
    chat = &battleChatterEvent[event];
#if BATTLE_TUNING
    if (batChatterToTune != NULL && batChatterToTune != chat)
    {                                                       //if we're tuning only chatter event
        return(ERROR);                                      //no jouez-vous
    }
#endif
    if (universe.curPlayerIndex != ship->playerowner->playerIndex)
    {
        return(ERROR);
    }
    weight = batGlobalFrequencyModifier * (real32)chat->randomWeight;
    if (((real32)batRandom() < weight * (real32)BAT_RandomTotal) &&
        (ship->obj.cameraDistanceSquared < (real32)chat->maxDistance))
    {                                                       //if it passes random weight and distance tests
#if BATTLE_TEXT
        if(chat->texts != NULL)
        {
            battleSpeechText(chat->eventNumber, chat->texts);
        }
#endif
        chat->lastTimeSpoken = universe.totaltimeelapsed;
        chat->lastPlaceSpoken = ship->obj.posinfo.position;
        if (chat->eventNumber == 0x100080c)
        {
            return(speechEventQueue(NULL, chat->eventNumber,
                variable, SOUND_EVENT_DEFAULT, SOUND_EVENT_DEFAULT,
                universe.curPlayerIndex, linkTo,
                (real32)SOUND_EVENT_DEFAULT, SOUND_EVENT_DEFAULT));
        }
        return(speechEventQueue(ship, chat->eventNumber,
            variable, SOUND_EVENT_DEFAULT, SOUND_EVENT_DEFAULT,
            universe.curPlayerIndex, linkTo,
            (real32)SOUND_EVENT_DEFAULT, SOUND_EVENT_DEFAULT));
    }
    return(ERROR);
}

/*-----------------------------------------------------------------------------
    Name        : battleRepeatOK
    Description : CAT: the wavelength and proximity tests of battleCanChatterAtThisTime 0x004238b0 and
                  battleChatterFleetAttempt 0x00423a50. With the proximity squared, the exe compares the squared
                  distance with it and takes squared distance / proximity as the curve's input (HW1: distance /
                  proximity).
    Inputs      : chat - the event; where - where it would be said (NULL: no proximity test);
                  bothInWavelength - TRUE: the proximity test only runs inside the wavelength (battleCanChatter...)
    Return      : TRUE if it may be said
----------------------------------------------------------------------------*/
static bool32 battleRepeatOK(battleevent *chat, vector *where, bool32 bothInWavelength)
{
    real32 timeSinceLastSpoken;
    double result, distanceSq;

    timeSinceLastSpoken = universe.totaltimeelapsed - chat->lastTimeSpoken;
    if (timeSinceLastSpoken > (real32)chat->minWavelength)
    {                                                       //if it hasn't been spoken in a coon's age
        if (bothInWavelength)
        {
            return(TRUE);
        }
    }
    else
    {
        result = (double)(timeSinceLastSpoken / (real32)chat->minWavelength);
        if (chat->expWavelength != 1.0)
        {                                                   //find proper spot on probability curve
            result = pow(result, chat->expWavelength);
        }
        if (result * (double)BAT_RandomTotal <= (double)batRandom())
        {
            return(FALSE);
        }
    }
    if ((chat->minRepeatProximity > 0.0) && (where != NULL))
    {                                                       //if we should consider location of last time this event was spoken
        // [audit: the exe subtracts on the x87 stack (flds, fsubs) and squares and sums there, storing only the double
        //  sum (fstl; battleCanChatterAtThisTime 0x00423969..0x004239af, battleChatterFleetAttempt 0x00423b54..): the
        //  differences are not rounded to float first]
        distanceSq = ((double)where->x - (double)chat->lastPlaceSpoken.x) * ((double)where->x - (double)chat->lastPlaceSpoken.x) +
                     ((double)where->y - (double)chat->lastPlaceSpoken.y) * ((double)where->y - (double)chat->lastPlaceSpoken.y) +
                     ((double)where->z - (double)chat->lastPlaceSpoken.z) * ((double)where->z - (double)chat->lastPlaceSpoken.z);
        if (distanceSq < chat->minRepeatProximity)
        {
            result = distanceSq / fmathSqrtDouble(chat->minRepeatProximity);
            if (chat->expRepeatProximity != 1.0)
            {
                result = pow(result, chat->expRepeatProximity);
            }
            if (result * (double)BAT_RandomTotal <= (double)batRandom())
            {
                return(FALSE);
            }
        }
    }
    return(TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : battleCanChatterAtThisTime
    Description : See if a particular event will work at this time (i.e. it
                    has not been used around here recently)
    Inputs      : event - what event to consider
                  ship - what ship we would like to play it on.  If NULL,
                  camera's position will be used for location information if needed.
    Outputs     :
    Return      : TRUE if it's OK to chatter at this time.
----------------------------------------------------------------------------*/
bool32 battleCanChatterAtThisTime(sdword event, Ship *ship)
{
    battleevent *chat;

    dbgAssertOrIgnore(event >= 0 && event < BCE_LastBCE);
    chat = &battleChatterEvent[event];
#if BATTLE_TUNING
    if (batChatterToTune != NULL && batChatterToTune != chat)
    {                                                       //if we're tuning only one chatter event
        return(ERROR);                                      //no jouez-vous
    }
#endif
    return(battleRepeatOK(chat, (ship != NULL) ? &ship->obj.posinfo.position : &mrCamera->eyeposition, TRUE));
}

/*-----------------------------------------------------------------------------
    Name        : battleChatterFleetAttempt
    Description : Tries to play a fleet command battle chatter event.
                  CAT: the exe's version (battleChatterFleetAttempt 0x00423a50): status events are spaced (30 s
                  after any status event, 60 s after the same one), the weight test is the raw weight times the
                  frequency scale, and a status-suppressed event is not said in single player.
    Inputs      : linkTo - what speech event to link to, or SOUND_EVENT_DEFAULT
                  event - what event to play
                  variable - variable for speech event or SOUND_EVENT_DEFAULT
                  where - position info so that we can use proximity tuning.
                    May be NULL
    Outputs     :
    Return      : handle of queued speech event, or ERROR
    Note        : This function should only be called for the correct player as
                    there is no ship parameter to determine what player this
                    event belongs to.
----------------------------------------------------------------------------*/
sdword battleChatterFleetAttempt(sdword linkTo, sdword event, sdword variable, vector *where)
{
    battleevent *chat;
    real32 weight, wait;

    dbgAssertOrIgnore(event >= 0 && event < BCE_LastBCE);
    chat = &battleChatterEvent[event];
#if BATTLE_TUNING
    if (batChatterToTune != NULL && batChatterToTune != chat)
    {                                                       //if we're tuning only one chatter event
        return(ERROR);                                      //no jouez-vous
    }
#endif
    if (chat->statusUpdate)
    {
        wait = (chat == catBatLastStatusEvent) ? 60.0f : 30.0f;
        if (universe.totaltimeelapsed < catBatLastStatusTime + wait)
        {
            return(ERROR);
        }
    }

    if (!battleRepeatOK(chat, where, FALSE))
    {
        return(ERROR);
    }

    weight = batGlobalFrequencyModifier * (real32)chat->randomWeight;
    if ((real32)batRandom() < weight * (real32)BAT_RandomTotal)
    {                                                       //if it passes random weight test
#if BATTLE_TEXT
        if(chat->texts != NULL)
        {
            battleSpeechText(chat->eventNumber, chat->texts);
        }
#endif
        if (where)
        {                                                   //remember where it was spoken
            chat->lastPlaceSpoken = *where;
        }
        chat->lastTimeSpoken = universe.totaltimeelapsed;
        if (chat->statusUpdate)
        {
            catBatLastStatusTime = universe.totaltimeelapsed;
            catBatLastStatusEvent = chat;
        }
        if (!singlePlayerGame || !chat->statusSuppress)
        {
            return(speechEventQueue(NULL, chat->eventNumber,
                variable, SOUND_EVENT_DEFAULT, SOUND_EVENT_DEFAULT,
                universe.curPlayerIndex, linkTo,
                (real32)SOUND_EVENT_DEFAULT, SOUND_EVENT_DEFAULT));
        }
    }
    return(ERROR);
}

/*-----------------------------------------------------------------------------
    Name        : battleMouthPieceFind
    Description : Find a ship to do some talking.  The ship needs to be one of
                    the player's ships.
    Inputs      : ships - list of ships to look through
                  numShips - number of ships in list
    Outputs     :
    Return      : Pointer to one of the players' ships, or NULL if one could not be found.
----------------------------------------------------------------------------*/
Ship *battleMouthPieceFind(Ship **ships, sdword numShips)
{
    Ship *mouthPiece;
    sdword tries = 0;

    do
    {
        tries++;
        if (tries > batMouthpieceTries)
        {
            return(NULL);
        }
        mouthPiece = ships[batRandom() % numShips];
    }
    while ( mouthPiece->playerowner != universe.curPlayerPtr);

    dbgAssertOrIgnore(mouthPiece->header.objtype == OBJ_ShipType);
    return(mouthPiece);
}

/*-----------------------------------------------------------------------------
    Name        : battleMouthPieceFindAnother
    Description : Same as above, but it will exclude the specified ship from
                    the search.
    Inputs      : firstMouthPiece - the ship that we don't want to be talking
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
Ship *battleMouthPieceFindAnother(Ship *firstMouthPiece, Ship **ships, sdword numShips)
{
    Ship *mouthPiece;
    sdword tries = 0;

    if (numShips == 0)
    {
        return(NULL);
    }
    do
    {
        mouthPiece = ships[batRandom() % numShips];
        tries++;
        if (tries > batMouthpieceTries)
        {
            return(NULL);
        }
    }
    while ( mouthPiece->playerowner != universe.curPlayerPtr && mouthPiece != firstMouthPiece);

    dbgAssertOrIgnore(mouthPiece->header.objtype == OBJ_ShipType);
    return(mouthPiece);
}

/*-----------------------------------------------------------------------------
    Name        : battleShipOnMyTail
    Description : See if a ship is on the tail of another ship
    Inputs      : meShip - ship being followed
                  badGuyShip - ship [possibly] following us
    Outputs     :
    Return      : TRUE if the ship is on the tail
----------------------------------------------------------------------------*/
bool32 battleShipOnMyTail(Ship *meShip, Ship *badGuyShip)
{
    vector temp, bogeyPosition;
    matrix transpose;

    matCopyAndTranspose(&meShip->rotatable.rotinfo.coordsys, &transpose);//transpose of coordsys
    vecSub(temp, badGuyShip->obj.posinfo.position, meShip->obj.posinfo.position);//vector from us to them
    matMultiplyMatByVec(&bogeyPosition, &transpose, &temp); //badguy's position in our local space
    if (bogeyPosition.z < 0.0f)
    {                                           //if they're behind us
        //sort of a "Manhattan bogey attitude" check
        if (ABS(bogeyPosition.x) + ABS(bogeyPosition.y) < -bogeyPosition.z * batBogeyAspect)
        {                                       //and they're on our 6
            vecNegate(temp);                    //vector from them to us
            matCopyAndTranspose(&badGuyShip->rotatable.rotinfo.coordsys, &transpose);//transpose of coordsys
            matMultiplyMatByVec(&bogeyPosition, &transpose, &temp); //our position in badguy's local space
            if (bogeyPosition.z > 0.0f)
            {                                   //if they're ahead of us
                if (ABS(bogeyPosition.x) + ABS(bogeyPosition.y) < bogeyPosition.z * batBogeyAspect)
                {                               //and we're in their sights
                    return(TRUE);
                }
            }
        }
    }
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : battleAccountForLosses
    Description : Account for the loss of a ship on the compution of future BSN's
    Inputs      : ship - what ship was lost
                  battlePing - what ping it was lost from
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void battleAccountForLosses(Ship *ship, battleping *battlePing)
{
    real32 lossPower, lossValue;
    real32 timeSinceLoss;

    lossPower = gunShipFirePower(ship->staticinfo);
    lossValue = (real32)ship->staticinfo->buildCost;

    if (ship->playerowner == universe.curPlayerPtr)
    {                                                       //if it's one of our ships we're losing
        if (battlePing->friendlyValueRecentlyLost != 0.0f)
        {
            timeSinceLoss = universe.totaltimeelapsed - battlePing->whenFriendliesLost;
            if (timeSinceLoss != 0.0f)
            {
                battlePing->friendlyFirepowerRecentlyLost /= (real32)pow((double)timeSinceLoss, (double)batExpLossDevalue);
                battlePing->friendlyValueRecentlyLost /= (real32)pow((double)timeSinceLoss, (double)batExpLossDevalue);
            }
        }
        battlePing->friendlyFirepowerRecentlyLost += lossPower;
        battlePing->friendlyValueRecentlyLost += lossValue;
        battlePing->whenFriendliesLost = universe.totaltimeelapsed;
    }
    else
    {                                                       //else it's an enemy that was lost
        if (battlePing->enemyValueRecentlyLost != 0.0f)
        {
            timeSinceLoss = universe.totaltimeelapsed - battlePing->whenEnemiesLost;
            if (timeSinceLoss != 0.0f)
            {
                battlePing->enemyFirepowerRecentlyLost /= (real32)pow((double)timeSinceLoss, (double)batExpLossDevalue);
                battlePing->enemyValueRecentlyLost /= (real32)pow((double)timeSinceLoss, (double)batExpLossDevalue);
            }
        }
        battlePing->enemyFirepowerRecentlyLost += lossPower;
        battlePing->enemyValueRecentlyLost += lossValue;
        battlePing->whenEnemiesLost = universe.totaltimeelapsed;
    }
}

#define thisPing ((ping *)voidPing)
/*-----------------------------------------------------------------------------
    Name        : battlePingEvaluateNoFriendlies
    Description : Handle a battle ping with no friendlies.
    Inputs      : thisPing - the ping who's evaluation function we are being
                    called from.
                  battlePing - the battle ping we are evaluating
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void battlePingEvaluateNoFriendlies(void *voidPing, battleping *battlePing)
{
    ;//nothing to play in this case
}

/*-----------------------------------------------------------------------------
    Name        : battlePingEvaluateNoEnemies
    Description : Same as regular evaluation, except there are no enemies.
                    We've probably just won a battle, or maybe it's friendly
                    or allied fire.
                  CAT: the exe's version (battlePingEvaluateNoEnemies 0x00423f80): no mothership-class ship
                  talks; friendly and allied fire are reported by the fleet (by group number when the ship has
                  one); the victory line is the fleet's (BCE_BattleWon), then the group's; none in mission 13.
    Inputs      : thisPing - the ping who's evaluation function we are being
                    called from.
                  battlePing - the battle ping we are evaluating
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void battlePingEvaluateNoEnemies(void *voidPing, battleping *battlePing)
{
    Ship *mouthPiece = NULL;
    sdword handle;
    sdword index;
    sdword group;
    Ship *ship;
    bool32 bFriendlyFire = FALSE;

    //do a per-ship scan for some events
    for (index = 0; index < battlePing->nShips; index++)
    {
        ship = battlePing->ship[index];
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
        dbgAssertOrIgnore(ship->playerowner != NULL);
        if (ship->staticinfo->shipclass == CLASS_Mothership)
        {                                                   //don't even have the mothership talking
            continue;
        }
        if (ship->playerowner == universe.curPlayerPtr)
        {                                                   //if this is a friendly ship (thus we can hear it speak)
            //Are my guys someone shooting at me?
            if (bitTest(ship->chatterBits, BCB_FriendlyFire))
            {
                group = selHotKeyGroupNumberTest(ship);
                if (group != 0)
                {
                    battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_GroupFriendlyFire, group, NULL);
                }
                else
                {
                    battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_FriendlyFire, SOUND_EVENT_DEFAULT, NULL);
                }
                bitClear(ship->chatterBits, BCB_FriendlyFire);
                bFriendlyFire = TRUE;
            }
            //Are my allies someone shooting at me?
            if (bitTest(ship->chatterBits, BCB_AlliedFire))
            {
                group = selHotKeyGroupNumberTest(ship);
                if (group != 0)
                {                                           //if allies are attacking a selected group
                    battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_GroupAttackFromAlly, group, NULL);
                }
                else
                {                                           //not a group number; report from fleet command
                    battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_AttackFromAlly, SOUND_EVENT_DEFAULT, &ship->obj.posinfo.position);
                }
                bitClear(ship->chatterBits, BCB_AlliedFire);
                bFriendlyFire = TRUE;
            }
        }
    }
    //if there was no friendly fire, it must mean we've just won a battle
    if ((!singlePlayerGame || (singlePlayerGameInfo.currentMission != 13)) &&
        !bFriendlyFire && (battlePing->enemyValueRecentlyLost != 0.0f))
    {
        mouthPiece = battleMouthPieceFind(battlePing->ship, battlePing->nShips);
        handle = battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_BattleWon, SOUND_EVENT_DEFAULT, NULL);
        if ((handle == ERROR) && (mouthPiece != NULL) && (selHotKeyGroupNumberTest(mouthPiece) != 0))
        {                                               //if could not play the fleet's victory line
            battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_GroupVictory, selHotKeyGroupNumberTest(mouthPiece), &mouthPiece->obj.posinfo.position);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : battlePingEvaluate
    Description : Called from the corresponding ping's evaluation function,
                    evaluate the battle ping and maybe make it say something.
                  CAT: the exe's version (battlePingEvaluate 0x00424110): at most every 8 s per ping and only with
                  a non-exempt ship of the local player in it; the battle assessments are the fleet's lines
                  (BCE_StartBattle*, BCE_KickingButt, BCE_WinningBattle, BCE_NeutralBattle, BCE_LosingBattle /
                  Badly, BCE_NegativeBattle) with the ships' answers; the responses carry the speaker's type; a
                  retreating ship is marked (catRetreating) and the retreat is the fleet's; the damaged-capital line
                  is the fleet's, or the "PrinceOfSpace" Easter-egg line; the loss-dependent weights are fixed.
    Inputs      : thisPing - the ping who's evaluation function we are being
                    called from.
                  battlePing - the battle ping we are evaluating
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void battlePingEvaluate(void *voidPing, battleping *battlePing)
{
    sdword index;
    Ship *ship;
    real32 enemyPower = 0.0f, friendlyPower = 0.0f;
    real32 enemyValue = 0.0f, friendlyValue = 0.0f;
    real32 enemyStanding, battleStanding, weight;
    Ship *mouthPiece = NULL, *mothership, *other;
    sdword handle, tries;
    CommandToDo *command;
    vector diff;
    double saveChance[4];
    bool32 bHeard = FALSE;

    if (universe.totaltimeelapsed < battlePing->catLastEvaluated + 8.0f)
    {
        return;
    }
    battlePing->catLastEvaluated = universe.totaltimeelapsed;

    //is one of the local player's (non-exempt) ships in this battle?
    for (index = 0; index < battlePing->nShips; index++)
    {
        ship = battlePing->ship[index];
        if ((ship->playerowner == universe.curPlayerPtr) && !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))
        {
            bHeard = TRUE;
            break;
        }
    }
    if (!bHeard)
    {
        return;
    }

    //do a per-ship scan for some events
    for (index = 0; index < battlePing->nShips; index++)
    {
        ship = battlePing->ship[index];
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
        dbgAssertOrIgnore(ship->playerowner != NULL);
        if (ship->header.flags & SOF_Dead)
        {
            continue;   // dead ships shouldn't interfere
        }
        if (ship->staticinfo->shipclass == CLASS_Mothership)
        {                                                   //don't even have the mothership talking
            continue;
        }
        if (ship->playerowner == universe.curPlayerPtr)
        {                                                   //if this is a friendly ship (thus we can hear it speak)
            //count up the power and cost of your ships
            friendlyPower += gunShipFirePower(ship->staticinfo);
            friendlyValue += (real32)ship->staticinfo->buildCost;

            //leaders may make snaps at their wingmen
            if ((ship->tacticstype == Evasive) && (ship->attackvars.myWingmanIs != NULL) &&
                battleCanChatterAtThisTime(BCE_Wingman, ship))
            {
                battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_Wingman, ship, SOUND_EVENT_DEFAULT);
            }
            //wingman (or another party in the battle) may tell this ship that they have a tail
            if (ship->recentlyFiredUpon && battleShipOnMyTail(ship, ship->firingAtUs))
            {
                handle = ERROR;
                if (ship->tacticstype == Evasive && ship->attackvars.myWingmanIs != NULL)
                {                                       //the wingman should say it if possible
                    mouthPiece = ship->attackvars.myWingmanIs;
                    if (battleCanChatterAtThisTime(BCE_LeaderChased, mouthPiece))
                    {
                        handle = battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_LeaderChased, mouthPiece, SOUND_EVENT_DEFAULT);
                    }
                }
                else
                {                                       //otherwise anybody else could say it
                    mouthPiece = battleMouthPieceFindAnother(ship, battlePing->ship, battlePing->nShips);
                    if (mouthPiece != NULL && battleCanChatterAtThisTime(BCE_WingmanChased, mouthPiece))
                    {                                       //if there is someone to say there is a bogey
                        handle = battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_WingmanChased, mouthPiece, SOUND_EVENT_DEFAULT);
                    }
                }
                if ((handle != ERROR) && battleCanChatterAtThisTime(BCE_WingmanChasedRsp, ship))
                {
                    battleChatterAttempt(handle, BCE_WingmanChasedRsp, ship, ship->shiptype);
                }
            }
            //say that a ship is recently hit, someone may respond
            if ((ship->recentlyAttacked > BAT_ReallyRecentAttack) && battleCanChatterAtThisTime(BCE_WingmanHit, ship))
            {                                               //if attacked quite recently
                handle = battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_WingmanHit, ship, SOUND_EVENT_DEFAULT);
                if (handle != ERROR)
                {
                    if (ship->tacticstype == Evasive && ship->attackvars.myWingmanIs != NULL)
                    {                                       //the wingman should say it if possible
                        mouthPiece = ship->attackvars.myWingmanIs;
                    }
                    else
                    {                                       //otherwise anybody else could say it
                        mouthPiece = battleMouthPieceFindAnother(ship, battlePing->ship, battlePing->nShips);
                    }
                    if (mouthPiece != NULL && battleCanChatterAtThisTime(BCE_WingmanHitRsp, mouthPiece))
                    {
                        battleChatterAttempt(handle, BCE_WingmanHitRsp, mouthPiece, mouthPiece->shiptype);
                    }
                }
            }
            //I've lost my wingman... bummer
            if (bitTest(ship->chatterBits, BCB_WingmanLost))
            {
                if (battleCanChatterAtThisTime(BCE_WingmanDies, ship))
                {
                    battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_WingmanDies, ship, ship->shiptype);
                }
                bitClear(ship->chatterBits, BCB_WingmanLost);
            }
            //am I really hurtin'?
            if (bitTest(ship->chatterBits, BCB_DamageInTheRed))
            {
                if (bitTest(battleClassDependencies[ship->staticinfo->shipclass], BCD_Damaged) &&
                    !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20) &&
                    (!battleCanChatterAtThisTime(BCE_CapitalShipDamaged, ship) ||
                     (battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_CapitalShipDamaged, ship, ship->shiptype) == ERROR)) &&
                    battleCanChatterAtThisTime(BCE_CapitalDamagedSpecific, ship))
                {                                           //the ship could not say it: the fleet reports it
                    if (!catSpeechPrinceOfSpace)
                    {
                        battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_CapitalDamagedSpecific, ship->shiptype, &ship->obj.posinfo.position);
                    }
                    else
                    {                                       // CAT: the "PrinceOfSpace" Easter egg (cat_speechEasterEggCheck)
                        speechEventQueue(NULL, 0x200135e, -1, -1, 0, -1, -1, 5.0f, -1);
                    }
                }
                bitClear(ship->chatterBits, BCB_DamageInTheRed);
            }
            //see if we're leaving the battle area
            command = ship->command;                        //get the ship's command
            if ((command != NULL) && (command->ordertype.order == COMMAND_MOVE))
            {
                //... see if he's moving out of the ping
                // CAT: the current point of the move path (+0x20 + 12 x curPoint)
                vecSub(diff, thisPing->centre, command->move.points[command->move.curPoint]);
                if (fmathSqrtDouble((double)vecMagnitudeSquared(diff)) > (double)batRetreatModifier * (double)battlePing->radius)
                {                                   //if this ship is moving out of the ping
                    ship->shipOrDerelict.catRetreating = TRUE;
                    handle = battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_Retreat, SOUND_EVENT_DEFAULT, NULL);
                    if (handle != ERROR)
                    {
                        battleChatterFleetAttempt(handle, BCE_RetreatRsp, SOUND_EVENT_DEFAULT, NULL);
                    }
                }
            }
        }
        else
        {                                                   //not a player ship
            if (allianceIsShipAlly(ship, universe.curPlayerPtr))
            {                                               //!!! consider your allies in the battle
                friendlyPower += gunShipFirePower(ship->staticinfo);
                friendlyValue += (real32)ship->staticinfo->buildCost;
            }
            else
            {
                enemyPower += gunShipFirePower(ship->staticinfo);
                enemyValue += (real32)ship->staticinfo->buildCost;
            }
        }
    }

    //compute the battle status number (BSN)
    enemyStanding = batStandingEnemyPower * enemyPower + batStandingEnemyValue * enemyValue;
    //save the chances of some events because they will have their randomWeight's modified
    saveChance[0] = battleChatterEvent[BCE_LosingBattle].randomWeight;
    saveChance[1] = battleChatterEvent[BCE_LosingBadly].randomWeight;
    saveChance[2] = battleChatterEvent[BCE_WinningBattle].randomWeight;
    saveChance[3] = battleChatterEvent[BCE_KickingButt].randomWeight;
    // CAT: after recent losses the losing (winning) lines get the standing-value coefficient, 1.0 without any value
    //      (HW1 computed a chance from the decayed losses)
    if (battlePing->friendlyValueRecentlyLost != 0.0f)
    {
        weight = (friendlyValue != 0.0f) ? batStandingFriendlyValue : 1.0f;
        battleChatterEvent[BCE_LosingBattle].randomWeight = (double)weight;
        battleChatterEvent[BCE_LosingBadly].randomWeight = (double)weight;
    }
    if (battlePing->enemyValueRecentlyLost != 0.0f)
    {
        weight = (enemyValue != 0.0f) ? batStandingEnemyValue : 1.0f;
        battleChatterEvent[BCE_WinningBattle].randomWeight = (double)weight;
        battleChatterEvent[BCE_KickingButt].randomWeight = (double)weight;
    }

    if (enemyStanding > 0.0f)
    {
        battleStanding = (batStandingFriendlyPower * friendlyPower + batStandingFriendlyValue * friendlyValue) / enemyStanding;
    }
    else
    {
        battleStanding = -1.0f;
    }

    // the first of up to batMouthpieceTries random ships of the local player does the talking
    mouthPiece = NULL;
    for (tries = 1; tries <= batMouthpieceTries; tries++)
    {
        other = battlePing->ship[batRandom() % battlePing->nShips];
        if (other->playerowner == universe.curPlayerPtr)
        {
            mouthPiece = other;
            break;
        }
    }

    if (mouthPiece != NULL)
    {                                                       //found someone to do the talking; now make him say something
        handle = ERROR;
        if (battleStanding > batBSNKickingButt)
        {                                                   //> kickingbutt
            if (!singlePlayerGame || (singlePlayerGameInfo.currentMission != 13))
            {
                handle = battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_StartBattleAdvantaged, SOUND_EVENT_DEFAULT, NULL);
                if (handle != ERROR)
                {
                    battleChatterFleetAttempt(handle, BCE_StartBattleAdvantagedRsp, SOUND_EVENT_DEFAULT, NULL);
                }
                else
                {
                    handle = battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_KickingButt, SOUND_EVENT_DEFAULT, NULL);
                    if (handle != ERROR)
                    {
                        other = battleMouthPieceFindAnother(mouthPiece, battlePing->ship, battlePing->nShips);
                        if (other != NULL)
                        {
                            battleChatterAttempt(handle, BCE_KickingButtRsp, other, other->shiptype);
                        }
                    }
                }
            }
        }
        else if (battleStanding > batBSNWinning)
        {                                                   //winning..kickingButt
            handle = battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_StartBattleAdvantaged, SOUND_EVENT_DEFAULT, NULL);
            if (handle != ERROR)
            {
                battleChatterFleetAttempt(handle, BCE_StartBattleAdvantagedRsp, SOUND_EVENT_DEFAULT, NULL);
            }
            else
            {
                handle = battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_WinningBattle, SOUND_EVENT_DEFAULT, NULL);
                if (handle != ERROR)
                {
                    other = battleMouthPieceFindAnother(mouthPiece, battlePing->ship, battlePing->nShips);
                    if (other != NULL)
                    {
                        battleChatterAttempt(handle, BCE_WinningBattleRsp, other, other->shiptype);
                    }
                }
                else
                {
                    handle = battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_PositiveBattle, SOUND_EVENT_DEFAULT, NULL);
                    if (handle != ERROR)
                    {
                        battleChatterAttempt(handle, BCE_PositiveBattle_More, mouthPiece, mouthPiece->shiptype);
                    }
                }
            }
        }
        else if (battleStanding > batBSNNeutral)
        {                                                   //neutral..winning
            handle = battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_StartBattleFair, SOUND_EVENT_DEFAULT, NULL);
            if (handle != ERROR)
            {
                battleChatterFleetAttempt(handle, BCE_StartBattleFairRsp, SOUND_EVENT_DEFAULT, NULL);
            }
            else if (battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_NeutralBattle, SOUND_EVENT_DEFAULT, NULL) == ERROR)
            {
                battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_NeutralBattle_More, SOUND_EVENT_DEFAULT, NULL);
            }
        }
        else if ((battleStanding > batBSNLosing) || (battleStanding > 0.0f))
        {                                                   //losing..neutral, or losing badly
            //is the mothership under heavy fire?
            mothership = universe.curPlayerPtr->PlayerMothership;
            if ((mothership != NULL) && selShipInSelection(battlePing->ship, battlePing->nShips, mothership) &&
                battleCanChatterAtThisTime(BCE_MothershipLargeAttack, mothership))
            {
                handle = battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_MothershipLargeAttack, mothership, mothership->shiptype);
            }
            if (handle != ERROR)
            {                                               //the mothership said so: someone answers
                if (battleCanChatterAtThisTime(BCE_MothershipLargeAttackResp, mouthPiece))
                {
                    battleChatterAttempt(handle, BCE_MothershipLargeAttackResp, mouthPiece, mothership->shiptype);
                }
            }
            else
            {
                handle = battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_StartBattleDisadvant, SOUND_EVENT_DEFAULT, NULL);
                if (handle != ERROR)
                {
                    battleChatterFleetAttempt(handle, BCE_StartBattleDisadvantRsp, SOUND_EVENT_DEFAULT, NULL);
                }
                else if (battleStanding > batBSNLosing)
                {                                           //losing..neutral
                    handle = battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_LosingBattle, SOUND_EVENT_DEFAULT, NULL);
                    if (handle != ERROR)
                    {
                        other = battleMouthPieceFindAnother(mouthPiece, battlePing->ship, battlePing->nShips);
                        if (other != NULL)
                        {
                            battleChatterAttempt(handle, BCE_LosingBattleRsp, other, other->shiptype);
                        }
                    }
                    else if (battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_LosingBattle_More, SOUND_EVENT_DEFAULT, NULL) == ERROR)
                    {
                        handle = battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_NegativeBattle, SOUND_EVENT_DEFAULT, NULL);
                        other = battleMouthPieceFindAnother(mouthPiece, battlePing->ship, battlePing->nShips);
                        if ((other != NULL) && (handle != ERROR) && battleCanChatterAtThisTime(BCE_NegativeBattle_More, other) &&
                            (battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_NegativeBattle_More, other, other->shiptype) == ERROR))
                        {
                            battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_NegativeBattle_More2, SOUND_EVENT_DEFAULT, NULL);
                        }
                    }
                }
                else
                {                                           //< losing (losing badly)
                    handle = battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_LosingBadly, SOUND_EVENT_DEFAULT, NULL);
                    if (handle == ERROR)
                    {
                        handle = battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_LosingBadly_More, mouthPiece, mouthPiece->shiptype);
                    }
                    if (handle != ERROR)
                    {
                        other = battleMouthPieceFindAnother(mouthPiece, battlePing->ship, battlePing->nShips);
                        if (other != NULL)
                        {
                            battleChatterAttempt(handle, BCE_LosingBadlyRsp, other, other->shiptype);
                        }
                    }
                }
            }
        }
    }
    //restore these previously saved randomWeight values
    battleChatterEvent[BCE_LosingBattle].randomWeight  = saveChance[0];
    battleChatterEvent[BCE_LosingBadly].randomWeight   = saveChance[1];
    battleChatterEvent[BCE_WinningBattle].randomWeight = saveChance[2];
    battleChatterEvent[BCE_KickingButt].randomWeight   = saveChance[3];
}
#undef thisPing

/*-----------------------------------------------------------------------------
    Name        : battleChatterFrequencySet
    Description : Set the battle chatter frequency
                  CAT: a plain scale, freq100 / 100 (battleChatterFrequencySet 0x00424d80); HW1's pow(2, x) curve
                  between batGlobalFrequencyMin and Max is gone
    Inputs      : freq100 - value from 0 to 100 (compatible with FE sliders)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void battleChatterFrequencySet(sdword freq100)
{
    batGlobalFrequencyModifier = (real32)((double)freq100 * 0.01);
}

/*-----------------------------------------------------------------------------
    Name        : battleShipDyingWithTimeToScream
    Description : Attempt to play one or more death speech events for a given ship's death.
                  CAT: the exe's version (battleShipDyingWithTimeToScream 0x00424da0): heavy cruisers, carriers,
                  destroyers, frigates and resource ships say BCE_CapitalShipDies with the fleet's report after
                  (only the fleet's line, without the ship, when the camera is 5000 or more from the nearest ship
                  in the main screen); corvettes, fighters and micro ships try the same line, then
                  BCE_WingmanDies; support-unit exempt ships say nothing.
    Inputs      : ship - ship that is dying
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void battleShipDyingWithTimeToScream(Ship *ship)
{
    sdword handle;

    if (bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))
    {
        return;
    }

    switch (ship->staticinfo->shipclass)
    {
        case CLASS_HeavyCruiser:
        case CLASS_Carrier:
        case CLASS_Destroyer:
        case CLASS_Frigate:
        case CLASS_Resource:
            if (battleCanChatterAtThisTime(BCE_CapitalShipDies, ship))
            {
                if (mrRenderMainScreen && (nearestShipDistance >= 5000.0f))
                {
                    speechEventQueue(NULL, 0x100080c, ship->shiptype, -1, 0, ship->playerowner->playerIndex, -1, 5.0f, -1);
                    return;
                }
                handle = battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_CapitalShipDies, ship, ship->shiptype);
                if (handle != ERROR)
                {
                    battleChatterFleetAttempt(handle, BCE_CapitalShipDiesReport, ship->shiptype, &ship->obj.posinfo.position);
                }
            }
            break;
        case CLASS_Corvette:
        case CLASS_Fighter:
        case CLASS_Micro:
            if ((!battleCanChatterAtThisTime(BCE_CapitalShipDies, ship) ||
                 (battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_CapitalShipDies, ship, ship->shiptype) == ERROR)) &&
                battleCanChatterAtThisTime(BCE_WingmanDies, ship))
            {
                battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_WingmanDies, ship, ship->shiptype);
            }
            break;
        default:
            break;
    }
}

/*-----------------------------------------------------------------------------
    Name        : battleLaunchWelcomeExchange
    Description : Play the lauch/welcome speech event exchange on launch of a
                    new super-capital ship.
                  CAT: the fleet's lines (battleLaunchWelcomeExchange 0x00424ef0, REWRITTEN; called by
                  cat_cmBuildJobFinished with the class and type): BCE_CapitalShipLaunched and its welcome, or
                  the build report 0x200103d with the type when the chatter does not play.
    Inputs      : shipclass - class of the ship built; variable - its type
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void battleLaunchWelcomeExchange(sdword shipclass, sdword variable)
{
    sdword handle;

    if (!bitTest(battleClassDependencies[shipclass], BCD_LaunchWelcome))
    {
         return;
    }
    handle = battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_CapitalShipLaunched, SOUND_EVENT_DEFAULT, NULL);
    if (handle != ERROR)
    {
        battleChatterFleetAttempt(handle, BCE_CapitalShipWelcomed, SOUND_EVENT_DEFAULT, NULL);
        return;
    }
    speechEventQueue(NULL, 0x200103d, variable, -1, 0, universe.curPlayerIndex, -1, 5.0f, -1);
}
