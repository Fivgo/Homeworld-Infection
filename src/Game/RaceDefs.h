#ifndef ___RACEDEFS_H
#define ___RACEDEFS_H

// CAT: Cataclysm's 7 races (shipraceinfo 0x008899c0 "RACE_SECT".."RACE_DERELICT"; CET game/Game/RaceDefs.h)
#define RACE_Sect       0
#define RACE_Beast      1
#define RACE_Kushan     2
#define RACE_Taiidan    3
#define RACE_P1         4
#define RACE_Traders    5
#define RACE_Derelict   6
#define NUM_RACES       (RACE_Derelict + 1)

// CAT: HW1 race names, kept only so that HW1 code not yet ported compiles (M2-R2 stage 1,
//      docs/m2-r2-identifiers.md). R1/R2 were HW1's two playable races, ids 0/1; Cataclysm's playable races
//      Sect/Beast have the same ids, so R1/R2 keep their values. P1 and Traders map to Cataclysm's races of the same
//      name. P2 and P3 do not exist in Cataclysm: their ids lie above every race and NSR_* value (ObjTypes.h), so no
//      data can produce them; never index a NUM_RACES table with them.
#define R1		RACE_Sect
#define R2		RACE_Beast
#define P1		RACE_P1
#define Traders	RACE_Traders
#define P2		16
#define P3		17

#endif
