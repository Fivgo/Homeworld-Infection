/*=============================================================================
    Name    : OracleDump.h
    Purpose : Definitions for OracleDump.c, the hwi-engine side of the checksum
              oracle (.hwo stream; docs/m2-checksum-oracle.md section 5,
              docs/m2-r2-oracle.md).

    Created 2026-10-10 for Homeworld-Infection (M2-R2). Instrumentation only:
    not Relic or Cataclysm code. This tree is REULA-licensed, see
    src/COPYING.REULA.
=============================================================================*/

#ifndef ___ORACLEDUMP_H
#define ___ORACLEDUMP_H

#include "Types.h"

// meson -Doracle=true (the default) defines HWI_ORACLE=1; -Doracle=false compiles the oracle out
#ifndef HWI_ORACLE
#define HWI_ORACLE 0
#endif

#if HWI_ORACLE

/*=============================================================================
    Functions:
=============================================================================*/

// command line (SDL/main.c)
void oracleSetDumpFile(char *string);           // /oracleDump <file>
void oracleSetSelfTest(char *string);           // /oracleSelfTest <ticks>
void oracleSetSelfTestPerturb(char *string);    // /oracleSelfTestPerturb <tick>

// self-test: the scripted lockstep start (utyGameSystemsInit), then the caller quits
bool32 oracleSelfTestRequested(void);
void oracleRunSelfTest(void);

// emit points; each is a no-op unless /oracleDump or /oracleSelfTest is given
bool32 oracleUnivUpdate(real32 phystimeelapsed);    // univUpdate + a 'T' record when the tick counter moved (H1)
void oracleEmitPacket(void *packet, uword packetnum, uword randcheck, real32 univcheck, udword blobcheck);   // H2
void oracleEmitGameStart(char *loadfilename);       // end of gameStart: 'E' + tick-0 'T' (H7)

#define ORACLE_UNIV_UPDATE(dt)  oracleUnivUpdate(dt)

#else

#define ORACLE_UNIV_UPDATE(dt)  univUpdate(dt)

#endif // HWI_ORACLE

#endif // ___ORACLEDUMP_H
