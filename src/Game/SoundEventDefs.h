#ifndef ___SOUNDEVENTDEFS_H
#define ___SOUNDEVENTDEFS_H

/* Flags for banks */
#define UI_Flag							256      //0x100
#define Gun_Flag						512         //0x200
#define ShipCmn_Flag					1024        //0x400
#define Ship_Flag						2048        //0x800
#define Exp_Flag						4096        //0x1000
#define Spec_Flag						8192        //0x2000
#define Hit_Flag						16384       //0x4000
#define Derelict_Flag					32768    //0x8000
#define SFX_Flag_Mask					65280    //0xFF00
#define SFX_Event_Mask					255      //0x00FF

/* Misc and UI events */
#define UI_SoundOfSpace					(0 + UI_Flag)
#define UI_BuildManager					(1 + UI_Flag)
#define UI_SensorsManager				(2 + UI_Flag)
#define UI_ResearchManager				(3 + UI_Flag)
#define UI_LaunchManager				(4 + UI_Flag)
#define UI_Trading						(5 + UI_Flag)

#define UI_ClickAccept					(6 + UI_Flag)
#define UI_ClickCancel					(7 + UI_Flag)
#define UI_Click						(8 + UI_Flag)
#define UI_ClickGeneric					(8 + UI_Flag)
#define UI_MovementGUIon				(9 + UI_Flag)
#define UI_MovementGUIoff				(10 + UI_Flag)
#define UI_MoveChimes					(11 + UI_Flag)
#define UI_TacOverlayOn					(12 + UI_Flag)
#define UI_TacOverlayOff				(13 + UI_Flag)
#define UI_IntelGUIon					(14 + UI_Flag)
#define UI_ChatMessage					(15 + UI_Flag)
#define UI_Letterbox					(16 + UI_Flag)
#define UI_Unletterbox					(17 + UI_Flag)
#define UI_GameEnd						(18 + UI_Flag)
#define UI_IntelGUIoff					(19 + UI_Flag)
#define UI_CrateFound					(20 + UI_Flag)
#define UI_ReceivingRUs					(21 + UI_Flag)
#define UI_MovementGUIaccept			(22 + UI_Flag)
#define UI_SensorsPing					(23 + UI_Flag)
#define UI_SensorsIntro					(24 + UI_Flag)
#define UI_SensorsExit					(25 + UI_Flag)
#define UI_ManagerIntro					(26 + UI_Flag)
#define UI_ManagerExit					(27 + UI_Flag)
#define UI_PingProximity				(28 + UI_Flag)
#define UI_PingNewShips					(29 + UI_Flag)
#define UI_PingBattle					(30 + UI_Flag)
#define UI_PingHyperspace				(31 + UI_Flag)
#define UI_RadioBeepStart				(32 + UI_Flag)
#define UI_RadioBeepEnd					(33 + UI_Flag)

#define NUM_UI_EVENTS					(UI_RadioBeepEnd - UI_Flag + 1)
// CAT (M2-R6 front end): Cataclysm's UIEvents.lut has 62 events x 2 races; the front-end controls play these ids
//      (names chosen here from their callers): a toggle pressed while checked / unchecked (uicToggleProcess 0x00534c00;
//      HW1's UI_TacOverlayOn / Off slots), a radio button or an arrow-key choice (uicRadioButtonProcess 0x00534dc0,
//      uicRightArrowProcess 0x00538fc0; HW1's UI_ChatMessage slot), the colour picker's two race buttons
#define CAT_UI_ToggleChecked			(12 + UI_Flag)      // 0x10c
#define CAT_UI_ToggleUnchecked			(13 + UI_Flag)      // 0x10d
#define CAT_UI_RadioClick				(15 + UI_Flag)      // 0x10f
#define CAT_UI_RaceOne					(58 + UI_Flag)      // 0x13a (the CP_RaceOne toggle)
#define CAT_UI_RaceTwo					(59 + UI_Flag)      // 0x13b (the CP_RaceTwo toggle)
// CAT (M2-R8 kasf library, sound): Cataclysm's ids of HW1's UI events, read from each exe push (soundEventPlay
//      0x005209f0); the trade screen's ambient is UI event 3 (tmTradeBegin 0x0052e566)
#define CAT_UI_TradingAmbient			(3 + UI_Flag)       // 0x103 (HW1's UI_Trading is 0x105)
#define CAT_UI_Letterbox				(35 + UI_Flag)      // 0x123 (kasfWideScreenIn 0x00497481; HW1 0x110)
#define CAT_UI_Unletterbox				(36 + UI_Flag)      // 0x124 (nisScissorOut 0x004d02d9, kasfWideScreenOut; HW1 0x111)
#define CAT_UI_CrateFound				(40 + UI_Flag)      // 0x128 (cratesPlayerFoundACrate 0x00460dc2; HW1 0x114)
#define CAT_UI_ChatMessage				(43 + UI_Flag)      // 0x12b (gcProcessGameChatPacket 0x00486dcc; HW1 0x10f)
#define CAT_UI_SensorsPing				(44 + UI_Flag)      // 0x12c (pingListDraw 0x004e8add; HW1 0x117)
#define CAT_UI_ManagerIntro				(47 + UI_Flag)      // 0x12f (tmTradeBegin 0x0052e558; HW1 0x11a)
#define CAT_UI_ManagerExit				(48 + UI_Flag)      // 0x130 (tmLeave 0x0052d7b7; HW1 0x11b)
#define CAT_UI_PingProximity			(49 + UI_Flag)      // 0x131 (pingListDraw 0x004e8b83; HW1 0x11c)
#define CAT_UI_PingNewShips				(50 + UI_Flag)      // 0x132 (pingListDraw 0x004e8bb6; HW1 0x11d)
#define CAT_UI_PingBattle				(51 + UI_Flag)      // 0x133 (pingListDraw 0x004e8b17; HW1 0x11e)
#define CAT_UI_PingHyperspace			(52 + UI_Flag)      // 0x134 (pingListDraw 0x004e8b50; HW1 0x11f)
// CAT: the UI ambients are events 0x100..0x107 (soundEventPlay 0x005209f0; HW1 up to UI_Trading)
#define CAT_UI_LAST_AMBIENT				(7 + UI_Flag)

/* Gun Events */
#define Gun_WeaponMove					(0 + Gun_Flag)
#define Gun_WeaponShot					(1 + Gun_Flag)
#define Gun_WeaponFireLooped			(2 + Gun_Flag)

#define NUM_GUN_EVENTS					(Gun_WeaponFireLooped - Gun_Flag + 1)

/* Common Ship Events */
#define ShipCmn_Engine					(0 + ShipCmn_Flag)
#define ShipCmn_Ambient					(1 + ShipCmn_Flag)
#define ShipCmn_RandomAmbient			(2 + ShipCmn_Flag)
#define ShipCmn_LightDamage				(3 + ShipCmn_Flag)
#define ShipCmn_MediumDamage			(4 + ShipCmn_Flag)
#define ShipCmn_HeavyDamage				(5 + ShipCmn_Flag)
#define ShipCmn_AnimOpening				(6 + ShipCmn_Flag)
#define ShipCmn_AnimClosing				(7 + ShipCmn_Flag)
#define ShipCmn_AnimDamagedOpen			(8 + ShipCmn_Flag)
#define ShipCmn_AnimDamagedClose		(9 + ShipCmn_Flag)

#define NUM_SHIP_CMN_EVENTS				(ShipCmn_AnimDamagedClose - ShipCmn_Flag + 1)

/* Ship Events */
#define Ship_Hyperdrive					(0 + Ship_Flag)
#define Ship_ResourceAsteroid			(1 + Ship_Flag)
#define Ship_ResourceDustCloud			(2 + Ship_Flag)
#define Ship_ResourceTransfer			(3 + Ship_Flag)
#define Ship_DroneLaunch				(4 + Ship_Flag)
#define Ship_DroneAcquire				(5 + Ship_Flag)
#define Ship_GravWellGenerator			(6 + Ship_Flag)
#define Ship_CloakingLoop				(7 + Ship_Flag)
#define Ship_CloakOn					(8 + Ship_Flag)
#define Ship_CloakOff					(9 + Ship_Flag)
#define Ship_ResearchLockOn				(10 + Ship_Flag)
#define Ship_HyperdriveOff				(11 + Ship_Flag)
#define Ship_MoshipDoorClosed			(12 + Ship_Flag)
#define Ship_Salvage					(13 + Ship_Flag)
#define Ship_SelectProbe				(14 + Ship_Flag)
#define Ship_SinglePlayerHyperspace		(15 + Ship_Flag)
#define Ship_DockGeneric				(16 + Ship_Flag)
#define Ship_RepairLoop					(17 + Ship_Flag)

#define NUM_SHIPS_EVENTS				(Ship_RepairLoop - Ship_Flag + 1)

/* Derelict Events */
#define Derelict_Ambience				(0 + Derelict_Flag)
#define Derelict_RandomAmbience			(1 + Derelict_Flag)

#define NUM_DERELICT_EVENTS				(Derelict_RandomAmbience - Derelict_Flag + 1)

/* Explosion Events */
#define Exp_DestDamageSmallFast			(0 + Exp_Flag)
#define Exp_DestMothership				(1 + Exp_Flag)
#define Exp_DestDamageLarge				(2 + Exp_Flag)
#define Exp_DestProjectileSmall			(3 + Exp_Flag)
#define Exp_DestProjectileLarge			(4 + Exp_Flag)
#define Exp_DestBeam					(5 + Exp_Flag)
#define Exp_PreExplosion				(6 + Exp_Flag)

#define NUM_EXP_EVENTS					(Exp_PreExplosion - Exp_Flag + 1)

/* Weapon Hit Events */
#define Hit_Nick						(0 + Hit_Flag)
#define Hit_Strike						(1 + Hit_Flag)
#define Hit_Ricochet					(2 + Hit_Flag)
#define Hit_PlasmaBomb					(3 + Hit_Flag)
#define Hit_Missle						(4 + Hit_Flag)
#define Hit_Mine						(5 + Hit_Flag)
#define Hit_IonCarving					(6 + Hit_Flag)

// CAT: an eighth hit event (the exe's effectHandle table holds 8 entries: SEinitHandles 0x005209b0 fills
//      0x00c0a120-0x00c0a180; defensefielddeflect.ebg plays (7 + Hit_Flag)); its name is not known
#define Hit_Cat7						(7 + Hit_Flag)

#define NUM_HIT_EVENTS					(Hit_Cat7 - Hit_Flag + 1)

#endif
