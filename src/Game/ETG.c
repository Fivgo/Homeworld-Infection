/*=============================================================================
    Name    : ETG.C
    Purpose : Effects Tool.  OF THE GODS!
        Parsing and execution code.

    Created 11/13/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "ETG.h"

#include <math.h>
#include <string.h>
#include <stdarg.h>

#include "Battle.h"
#include "BTG.h"
#include "Color.h"
#include "Damage.h"
#include "Debug.h"
#include "Eval.h"
#include "FastMath.h"
#include "File.h"
#include "glinc.h"
#include "HorseRace.h"
#include "mainrgn.h"
#include "Memory.h"
#include "Mesh.h"
#include "MEX.h"
#include "Particle.h"
#include "Randy.h"
#include "render.h"
#include "Select.h"
#include "SinglePlayer.h"     // CAT: cat_univRemoveEffectReferencesFromShips walks the hyperspace ether
#include "SoundEvent.h"
#include "StatScript.h"
#include "Task.h"
#include "texreg.h"
#include "Tweak.h"
#include "Types.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "utility.h"

// CAT: GoK's generated per-callable wrappers (functions.h, wrap.h, wrap_types.h, wrapped_unlisted_functions.h) are
//      gone: Cataclysm's callables take one argument-array pointer, which etgFunctionCall can call on any host

#ifdef _WIN32
    #include <windows.h>
#endif

#ifdef _MSC_VER
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif

/*=============================================================================
    Data:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Effect-event registry table
-----------------------------------------------------------------------------*/
etgevent etgEventTable[ETG_EventListLength];
sdword etgEventLoadCount = 0;
//sdword etgEventIndex = 0;

//data for error recovery during ETG parsing
bool32 etgErrorRecoverable = FALSE;
bool32 etgErrorEncountered = FALSE;
#define PARSEERROR      (etgErrorRecoverable && etgErrorEncountered)

/*-----------------------------------------------------------------------------
    Master dispatch table for processing effects:
-----------------------------------------------------------------------------*/
sdword etgNOP(Effect *effect, etgeffectstatic *stat, ubyte *opcode);
sdword etgVarCopy(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgVarAssign(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgFunctionCall(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgEnd(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgDelete(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgCompareVCI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgNotEqualVCI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgGreaterVCI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgGreaterEqualVCI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgLessVCI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgLessEqualVCI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgCompareVCF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgNotEqualVCF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgGreaterVCF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgGreaterEqualVCF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgLessVCF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgLessEqualVCF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgEqualVVI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgNotEqualVVI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgGreaterVVI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgGreaterEqualVVI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgLessVVI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgLessEqualVVI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgEqualVVF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgNotEqualVVF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgGreaterVVF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgGreaterEqualVVF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgLessVVF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgLessEqualVVF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgBranchTo(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgCallTo(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgReturnTo(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgAlternate(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgBetween(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgEffectorCI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgEffectorCF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgEffectorCCo(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgEffectorCCa(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgEffectorVF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgEffectorVCo(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgEffectorVCa(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgAt(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgAtVar(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgEvery(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgEveryVar(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgBefore(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgBeforeVar(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgAfter(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
sdword etgAfterVar(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode);
ophandleentry etgHandleTable[] =
{
/* EOP_Nop            */ {sizeof(etgnop), etgNOP},
/*//time-index code bl*/
/* EOP_Before         */ {sizeof(etgbeforeafterat), etgBefore},
/* EOP_BeforeVar      */ {sizeof(etgbeforeafterat), etgBeforeVar},
/* EOP_After          */ {sizeof(etgbeforeafterat), etgAfter},
/* EOP_AfterVar       */ {sizeof(etgbeforeafterat), etgAfterVar},
/* EOP_At             */ {sizeof(etgbeforeafterat), etgAt},
/* EOP_AtVar          */ {sizeof(etgbeforeafterat), etgAtVar},
/* EOP_Between        */ {sizeof(etgbetween), etgBetween},
/* EOP_Every          */ {sizeof(etgbeforeafterat), etgEvery},
/* EOP_EveryVar       */ {sizeof(etgbeforeafterat), etgEveryVar},
/* EOP_EffectorCCI    */ {sizeof(etgeffector), NULL},
/* EOP_EffectorCCF    */ {sizeof(etgeffector), NULL},
/* EOP_EffectorCCCo   */ {sizeof(etgeffector), NULL},
/* EOP_EffectorCCCa   */ {sizeof(etgeffector), NULL},
/* EOP_EffectorCVI    */ {sizeof(etgeffector), NULL},
/* EOP_EffectorCVF    */ {sizeof(etgeffector), NULL},
/* EOP_EffectorCVCo   */ {sizeof(etgeffector), NULL},
/* EOP_EffectorCVCa   */ {sizeof(etgeffector), NULL},
/* EOP_EffectorVCI    */ {sizeof(etgeffector), NULL},
/* EOP_EffectorVCF    */ {sizeof(etgeffector), NULL},
/* EOP_EffectorVCCo   */ {sizeof(etgeffector), NULL},
/* EOP_EffectorVCCa   */ {sizeof(etgeffector), NULL},
/* EOP_EffectorVVI    */ {sizeof(etgeffector), NULL},
/* EOP_EffectorVVF    */ {sizeof(etgeffector), NULL},
/* EOP_EffectorVVCo   */ {sizeof(etgeffector), NULL},
/* EOP_EffectorVVCa   */ {sizeof(etgeffector), NULL},
/* EOP_EffectorVI,    */ {sizeof(etgseffector), NULL},
/* EOP_EffectorVF,    */ {sizeof(etgseffector), etgEffectorVF},
/* EOP_EffectorVCo    */ {sizeof(etgseffector), etgEffectorVCo},
/* EOP_EffectorVCa    */ {sizeof(etgseffector), etgEffectorVCa},
/* EOP_EffectorCI,    */ {sizeof(etgseffector), etgEffectorCI},
/* EOP_EffectorCF,    */ {sizeof(etgseffector), etgEffectorCF},
/* EOP_EffectorCCo    */ {sizeof(etgseffector), etgEffectorCCo},
/* EOP_EffectorCCa    */ {sizeof(etgseffector), etgEffectorCCa},
/*//process-control (a*/
/* EOP_Delete         */ {sizeof(etgprocesscontrol), etgDelete},
/* EOP_End            */ {sizeof(etgprocesscontrol), etgEnd},
/* EOP_Yeild          */ {sizeof(etgprocesscontrol), NULL},
/*//branching         */
/* EOP_Goto           */ {sizeof(etgbranch), etgBranchTo},
/* EOP_Call           */ {sizeof(etgbranch), etgCallTo},
/* EOP_Return         */ {sizeof(etgreturn), etgReturnTo},
/* EOP_Spawn          */ {sizeof(etgnop), NULL},       //!!! needed?
/* EOP_Start          */ {sizeof(etgnop), NULL},       //!!! needed?
/*//conditionals      */
/* EOP_EqualVCI       */ {sizeof(etgconditional), etgCompareVCI     },
/* EOP_NotEqualVCI    */ {sizeof(etgconditional), etgNotEqualVCI    },
/* EOP_GreaterVCI     */ {sizeof(etgconditional), etgGreaterVCI     },
/* EOP_GreatorEqualVCI*/ {sizeof(etgconditional), etgGreaterEqualVCI},
/* EOP_LessVCI        */ {sizeof(etgconditional), etgLessVCI        },
/* EOP_LessEqualVCI   */ {sizeof(etgconditional), etgLessEqualVCI   },
/* EOP_EqualVVI       */ {sizeof(etgconditional), etgEqualVVI       },
/* EOP_NotEqualVVI    */ {sizeof(etgconditional), etgNotEqualVVI    },
/* EOP_GreaterVVI     */ {sizeof(etgconditional), etgGreaterVVI     },
/* EOP_GreatorEqualVVI*/ {sizeof(etgconditional), etgGreaterEqualVVI},
/* EOP_LessVVI        */ {sizeof(etgconditional), etgLessVVI        },
/* EOP_LessEqualVVI   */ {sizeof(etgconditional), etgLessEqualVVI   },
/* EOP_EqualVCF       */ {sizeof(etgconditional), etgCompareVCF     },
/* EOP_NotEqualVCF    */ {sizeof(etgconditional), etgNotEqualVCF    },
/* EOP_GreaterVCF     */ {sizeof(etgconditional), etgGreaterVCF     },
/* EOP_GreatorEqualVCF*/ {sizeof(etgconditional), etgGreaterEqualVCF},
/* EOP_LessVCF        */ {sizeof(etgconditional), etgLessVCF        },
/* EOP_LessEqualVCF   */ {sizeof(etgconditional), etgLessEqualVCF   },
/* EOP_EqualVVF       */ {sizeof(etgconditional), etgEqualVVF       },
/* EOP_NotEqualVVF    */ {sizeof(etgconditional), etgNotEqualVVF    },
/* EOP_GreaterVVF     */ {sizeof(etgconditional), etgGreaterVVF     },
/* EOP_GreatorEqualVVF*/ {sizeof(etgconditional), etgGreaterEqualVVF},
/* EOP_LessVVF        */ {sizeof(etgconditional), etgLessVVF        },
/* EOP_LessEqualVVF   */ {sizeof(etgconditional), etgLessEqualVVF   },
/* EOP_ZeroVI         */ {sizeof(etgconditional), NULL},
/* EOP_NotZeroVI      */ {sizeof(etgconditional), NULL},
/* EOP_ZeroVF         */ {sizeof(etgconditional), NULL},
/* EOP_NotZeroVF      */ {sizeof(etgconditional), NULL},
/*//general           */
/* EOP_VariableCopy   */ {sizeof(etgvariablecopy), etgVarCopy},
/* EOP_VariableAssign */ {sizeof(etgvariablecopy), etgVarAssign},
/* EOP_Function       */ {sizeof(etgfunctioncall), etgFunctionCall},
/* EOP_Alternate      */ {sizeof(etgdecision),     etgAlternate},
/* EOP_Case           */ {sizeof(etgdecision),     NULL},
/* EOP_LastOp         */ {0, NULL},
};
//#if sizeof(opHandleTable) / sizeof(ophandleentry) != EOP_LastOp + 1
//#error opHandleTable wrong size
//#endif

/*-----------------------------------------------------------------------------
    Master table for parsing opcodes:
-----------------------------------------------------------------------------*/
sdword etgLineNumberUpdate(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgNOPCreate(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgVariableBlockOpen(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgStartupBlockOpen(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgEachFrameBlockOpen(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgTimeIndexBlockOpen(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgNewInteger(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgNewFloat(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgBlockOpen(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgBlockClose(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgConditional(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgElse(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgNewLocalLabel(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgLabelCall(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgLabelGoto(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgReturn(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgNewEventParse(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgEndOpcode(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgYeildOpcode(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgDeleteOpcode(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgAternateStart(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgAternateSet(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgTimeBlockOpen(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgTimeIndexDefine(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgNParticleBlocksSet(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgNHistorySet(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgNewRGB(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgNewRGBA(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgSubTextureDWORD(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgMorphAnimDWORD(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgSetTextureParse(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgSpawnParse(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgAtDefine(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgBetweenDefine(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgCreateParticlesParse(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgCreateEffectsParse(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret);
sdword etgSetMeshParse(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
sdword etgWorldRenderSet(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
//sdword etgForceVisibleSet(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret);
#define parseEntry(name, function, flags) {name, flags, function}
opparseentry etgParseTable[] =
{
    parseEntry("nop",           etgNOPCreate, 0),
//conditionals
    parseEntry("if",            etgConditional, PTF_Parameters),
    parseEntry("else",          etgElse, 0),
//labels
    parseEntry("label",         etgNewLocalLabel, PTF_Parameters),
/*
    parseEntry("exlabel",       etgNewExportLabel, PTF_Parameters),
*/
    parseEntry("eventStart",    etgNewEventParse, PTF_Parameters),
    parseEntry("variable",      etgVariableBlockOpen, 0),
//overall structure/syntax
    parseEntry("{",             etgBlockOpen, 0),
    parseEntry("}",             etgBlockClose, 0),
//modifications to the effect
    parseEntry("startup",       etgStartupBlockOpen, 0),
    parseEntry("eachFrame",     etgEachFrameBlockOpen, 0),
    parseEntry("timeIndex",     etgTimeIndexBlockOpen, 0),
    parseEntry("particleBlocks",etgNParticleBlocksSet, PTF_Parameters),
    parseEntry("maxFrequency",  etgNHistorySet, PTF_Parameters),
//    parseEntry("setForceVisible", etgForceVisibleSet, 0),
    parseEntry("worldRender",   etgWorldRenderSet, 0),
//variable declaration
    parseEntry("int",           etgNewInteger, PTF_RestOfLine),
    parseEntry("float",         etgNewFloat, PTF_RestOfLine),
    parseEntry("RGB",           etgNewRGB, PTF_RestOfLine),
    parseEntry("RGBA",          etgNewRGBA, PTF_RestOfLine),
    parseEntry("subTextureDWORD",etgSubTextureDWORD, PTF_Parameters),
    parseEntry("morphAnimDWORD",etgMorphAnimDWORD, PTF_Parameters),
    parseEntry("setTexture",    etgSetTextureParse, PTF_Parameters),
    parseEntry("modifyTexture", etgSetTextureParse, PTF_Parameters),
    parseEntry("setMesh",       etgSetMeshParse, PTF_Parameters),
/*
    parseEntry("vector",        etgNewVector, PTF_RestOfLine),
//time-index block comparisons
*/
    parseEntry("at",            etgAtDefine, PTF_Parameters),
    parseEntry("every",         etgAtDefine, PTF_Parameters),
    parseEntry("before",        etgAtDefine, PTF_Parameters),
    parseEntry("after",         etgAtDefine, PTF_Parameters),
    parseEntry("between",       etgBetweenDefine, PTF_Parameters),
/*
//effector definition
    parseEntry("effect"",        etgEffectorDefine, PTF_Parameters),
*/
//time-index blocks
    parseEntry("timeBlock",     etgTimeBlockOpen, PTF_Parameters),
    parseEntry("time",          etgTimeIndexDefine, PTF_Parameters),
//branching
    parseEntry("goto",          etgLabelGoto, PTF_Parameters),
    parseEntry("call",          etgLabelCall, PTF_Parameters),
    parseEntry("return",        etgReturn, 0),
//process control
    parseEntry("end",           etgEndOpcode, PTF_Parameters),
    parseEntry("delete",        etgDeleteOpcode, PTF_Parameters),
    parseEntry("yield",         etgYeildOpcode, PTF_Parameters),
//alternates
    parseEntry("alternate",     etgAternateStart, 0),
    parseEntry("alt",           etgAternateSet, PTF_Parameters),
//misc
    parseEntry("nop",           etgNOPCreate, 0),
    parseEntry("#line",         etgLineNumberUpdate, PTF_RestOfLine),
    parseEntry("spawn",         etgSpawnParse, PTF_Parameters),
/*
    parseEntry("createSprites", etgCreateParticlesParse, PTF_Parameters),
    parseEntry("createCircles", etgCreateParticlesParse, PTF_Parameters),
    parseEntry("createPoints",  etgCreateParticlesParse, PTF_Parameters),
    parseEntry("createLines",   etgCreateParticlesParse, PTF_Parameters),
    parseEntry("createMeshes",  etgCreateParticlesParse, PTF_Parameters),
*/
    parseEntry("createEffects", etgCreateEffectsParse, PTF_Parameters),
//********** functions/var copies are searched in a different table
    parseEntry(NULL, NULL, 0)
};

//master function parsing table
// CAT: every callable below takes one pointer to the argument array (ETG.h etgarg; etgFunctionCall 0x0046e7b0).
//      The exe's void callables return memsize 0 here so the call through etgcallable is well-defined.
memsize etgSetDefaults(etgarg *args);
memsize etgInc(etgarg *args);
memsize etgDec(etgarg *args);
memsize etgFdiv(etgarg *args);
memsize etgFmult(etgarg *args);
memsize cat_etgFdegsin(etgarg *args);
memsize etgFadd(etgarg *args);
memsize etgFsub(etgarg *args);
memsize etgAdd(etgarg *args);
memsize etgSub(etgarg *args);
memsize etgMult(etgarg *args);
memsize etgDiv(etgarg *args);
memsize etgSin(etgarg *args);
memsize etgCos(etgarg *args);
memsize etgCreateCircles(etgarg *args);
memsize etgCreateSprites(etgarg *args);
memsize etgCreatePoints(etgarg *args);
memsize etgCreateMeshes(etgarg *args);
memsize etgCreateLines(etgarg *args);
memsize cat_etgCreateEBolts(etgarg *args);
memsize cat_etgCreateLensFlares(etgarg *args);
memsize etgCreateEffects(etgarg *args);
memsize etgParentShipDelete(etgarg *args);
memsize etgParentShipHide(etgarg *args);
memsize cat_etgParentShipOmitFromRenderList(etgarg *args);
memsize etgChatterEventPlay(etgarg *args);
memsize etgDeathCriesPlay(etgarg *args);
memsize etgDamageDone(etgarg *args);
memsize etgSpawnNewEffect(etgarg *args);
memsize etgEffOffsetLOF(etgarg *args);
memsize etgEffChangeLOF(etgarg *args);
memsize etgEffOffsetR(etgarg *args);
memsize etgEffOffsetXYZ(etgarg *args);
memsize etgEffOffsetVelLOF(etgarg *args);
memsize etgEffOffsetSpin(etgarg *args);
memsize etgEffOffsetTime(etgarg *args);
memsize etgEffOffsetDrag(etgarg *args);
memsize etgEffDrag(etgarg *args);
memsize etgEffAttachParent(etgarg *args);
memsize etgEffSpin(etgarg *args);
memsize etgEffDeltaSpin(etgarg *args);
memsize etgEffDefaults(etgarg *args);
memsize etgThisOffsetXYZ(etgarg *args);
memsize etgThisSetDrag(etgarg *args);
memsize etgThisScaleVelocity(etgarg *args);
memsize etgThisOffsetSpin(etgarg *args);
memsize etgThisOffsetDeltaSpin(etgarg *args);
memsize etgThisOffsetTime(etgarg *args);
memsize etgSetDrag(etgarg *args);
memsize etgModifyDrag(etgarg *args);
memsize etgSetDepthWrite(etgarg *args);
memsize etgSetColorA(etgarg *args);
memsize etgSetColor(etgarg *args);
memsize etgSetAnimation(etgarg *args);
memsize etgSetMorphAnimation(etgarg *args);
memsize etgIRandom(etgarg *args);
memsize etgCRandom(etgarg *args);
memsize etgCARandom(etgarg *args);
memsize etgFloat2Int(etgarg *args);
memsize etgInt2Float(etgarg *args);
memsize cat_etgColRed(etgarg *args);
memsize cat_etgColGreen(etgarg *args);
memsize cat_etgColBlue(etgarg *args);
memsize etgInts2Color(etgarg *args);
memsize etgInts2ColorA(etgarg *args);
memsize etgFloats2Color(etgarg *args);
memsize etgFloats2ColorA(etgarg *args);
memsize etgSetSpecular(etgarg *args);
memsize etgOwnerColorSchemeGet(etgarg *args);
memsize etgEffectVelocityGet(etgarg *args);
memsize cat_etgNullsub(etgarg *args);
memsize etgCreateCallbackSetup(etgarg *args);
//resolve functions:
void etgCreationResolve(etgeffectstatic *stat, etgfunctioncall *call);
void etgDepthWriteResolve(struct etgeffectstatic *stat, etgfunctioncall *call);

// CAT: one entry layout for every callable (GoK's GENERIC_ETGCALLFUNCTION wrap_ variants are gone)
#define funcEntryn(name, ret, function)                                      {name, function, ret, (ubyte)FALSE, ETG_VariableParams}
#define funcEntry0(name, ret, function)                                      {name, function, ret, (ubyte)FALSE, NULL, 0, {0,0,0,0,0,0,0,0,0}}
#define funcEntry1(name, ret, function, p0)                                  {name, function, ret, (ubyte)FALSE, NULL, 1, {p0}}
#define funcEntry2(name, ret, function, p0, p1)                              {name, function, ret, (ubyte)FALSE, NULL, 2, {p0, p1}}
#define funcEntry3(name, ret, function, p0, p1, p2)                          {name, function, ret, (ubyte)FALSE, NULL, 3, {p0, p1, p2}}
#define funcEntry4(name, ret, function, p0, p1, p2, p3)                      {name, function, ret, (ubyte)FALSE, NULL, 4, {p0, p1, p2, p3}}
#define funcEntry5(name, ret, function, p0, p1, p2, p3, p4)                  {name, function, ret, (ubyte)FALSE, NULL, 5, {p0, p1, p2, p3, p4}}
#define funcEntry6(name, ret, function, p0, p1, p2, p3, p4, p5)              {name, function, ret, (ubyte)FALSE, NULL, 6, {p0, p1, p2, p3, p4, p5}}
#define funcEntry7(name, ret, function, p0, p1, p2, p3, p4, p5, p6)          {name, function, ret, (ubyte)FALSE, NULL, 7, {p0, p1, p2, p3, p4, p5, p6}}
#define funcEntry8(name, ret, function, p0, p1, p2, p3, p4, p5, p6, p7)      {name, function, ret, (ubyte)FALSE, NULL, 7, {p0, p1, p2, p3, p4, p5, p6, p7}}

#define funcEntryThisn(name, ret, function)                                  {name, function, ret, (ubyte)TRUE,  ETG_VariableParams}
#define funcEntryThis0(name, ret, function)                                  {name, function, ret, (ubyte)TRUE,  NULL, 0, {0,0,0,0,0,0,0,0,0}}
#define funcEntryThis1(name, ret, function, p0)                              {name, function, ret, (ubyte)TRUE,  NULL, 1, {p0}}
#define funcEntryThis2(name, ret, function, p0, p1)                          {name, function, ret, (ubyte)TRUE,  NULL, 2, {p0, p1}}
#define funcEntryThis3(name, ret, function, p0, p1, p2)                      {name, function, ret, (ubyte)TRUE,  NULL, 3, {p0, p1, p2}}

#define funcEntryR1(name, ret, function, p0, resolve)                        {name, function, ret, (ubyte)FALSE, resolve, 1, {p0}}
#define funcEntryThisR2(name, ret, function, p0, p1, resolve)                {name, function, ret, (ubyte)TRUE,  resolve, 2, {p0, p1}}

/*-----------------------------------------------------------------------------
    CAT: the exe's function table (etgFunctionTable 0x00879808, 167 rows of 0x1C bytes, dumped from the exe), in its
    order and with its argument types. Against HW1/GoK: new rows setRandomOffsetArray, setTurbulance, setSegments,
    print, modifyAlphaValue, modifyBackgroundColor, modifyRotAngle, modifyScaleX, modifyScaleY, modifyTumble,
    createEBolts, createLensFlares, fdegsin, omitParentShipFromRenderList, colRed/colGreen/colBlue;
    modifyDeltaLength takes a third (int) argument; HW1's duplicate setSpriteDeltaRot/modifyAlphaMode rows are gone.
    The exe's setFramerate row points at partSetLoopFlag (0x004e3aa0, as setLoopFlag does), not partSetFramerate;
    kept. print and modifyBackgroundColor point at the exe's empty nullsub (0x0049ae30).
-----------------------------------------------------------------------------*/
opfunctionentry etgFunctionTable[] =
{
    //set properties of particle systems to be created
    funcEntry0("setDefaults", EVT_Void, etgSetDefaults),
    funcEntry1("setDrag", EVT_Void, etgSetDrag, EVT_Float),
    funcEntry1("setScale", EVT_Void, partSetScale, EVT_Float),
    funcEntry3("setXYZScale", EVT_Void, partSetXYZScale, EVT_Float, EVT_Float, EVT_Float),
    funcEntry1("setScaleDist", EVT_Void, partSetScaleDist, EVT_Float),
    funcEntry1("setDeltaScale", EVT_Void, partSetDeltaScale, EVT_Float),
    funcEntry1("setDeltaScaleDist", EVT_Void, partSetDeltaScaleDist, EVT_Float),
    funcEntry1("setOffsetLOF", EVT_Void, partSetOffsetLOF, EVT_Float),
    funcEntry1("setOffsetR", EVT_Void, partSetOffsetR, EVT_Float),
    funcEntry1("setOffsetTheta", EVT_Void, partSetOffsetTheta, EVT_Float),
    funcEntry1("setOffsetArray", EVT_Void, partSetOffsetArray, EVT_ConstLabel),
    funcEntry6("setRandomOffsetArray", EVT_Void, cat_partSetRandomOffsetArray, EVT_Float, EVT_Float, EVT_Float, EVT_Float, EVT_Float, EVT_Float),
    funcEntry1("setDeltaLOF", EVT_Void, partSetDeltaLOF, EVT_Float),
    funcEntry1("setDeltaLOFDist", EVT_Void, partSetDeltaLOFDist, EVT_Float),
    funcEntry2("setDeltaR", EVT_Void, partSetDeltaR, EVT_Float, EVT_Float),
    funcEntry2("setDeltaRDist", EVT_Void, partSetDeltaRDist, EVT_Float, EVT_Float),
    funcEntry1("setVelLOF", EVT_Void, partSetVelLOF, EVT_Float),
    funcEntry1("setVelLOFDist", EVT_Void, partSetVelLOFDist, EVT_Float),
    funcEntry1("setVelR", EVT_Void, partSetVelR, EVT_Float),
    funcEntry1("setVelRDist", EVT_Void, partSetVelRDist, EVT_Float),
    funcEntry1("setSpriteRot", EVT_Void, partSetAng, EVT_Float),
    funcEntry1("setSpriteRotAngle", EVT_Void, partSetAng, EVT_Float),
    funcEntry1("setSpriteDeltaRot", EVT_Void, partSetAngDelta, EVT_Float),
    funcEntry1("setDeltaVelLOF", EVT_Void, partSetDeltaVelLOF, EVT_Float),
    funcEntry1("setDeltaVelLOFDist", EVT_Void, partSetDeltaVelLOFDist, EVT_Float),
    funcEntry1("setDeltaVelR", EVT_Void, partSetDeltaVelR, EVT_Float),
    funcEntry1("setDeltaVelRDist", EVT_Void, partSetDeltaVelRDist, EVT_Float),
    funcEntry1("setSpriteRotDist", EVT_Void, partSetAngDist, EVT_Float),
    funcEntry1("setSpriteDeltaRotDist", EVT_Void, partSetAngDeltaDist, EVT_Float),
    funcEntry1("setColor", EVT_Void, etgSetColor, EVT_RGB),
    funcEntry3("setColorDist", EVT_Void, partSetColorDist, EVT_Float, EVT_Float, EVT_Float),
    funcEntry3("setDeltaColor", EVT_Void, partSetDeltaColor, EVT_Float, EVT_Float, EVT_Float),
    funcEntry3("setDeltaColorDist", EVT_Void, partSetDeltaColorDist, EVT_Float, EVT_Float, EVT_Float),
    funcEntry1("setColorA", EVT_Void, etgSetColorA, EVT_RGBA),
    funcEntry4("setColorADist", EVT_Void, partSetColorADist, EVT_Float, EVT_Float, EVT_Float, EVT_Float),
    funcEntry4("setDeltaColorA", EVT_Void, partSetDeltaColorA, EVT_Float, EVT_Float, EVT_Float, EVT_Float),
    funcEntry4("setDeltaColorADist", EVT_Void, partSetDeltaColorADist, EVT_Float, EVT_Float, EVT_Float, EVT_Float),
    funcEntry3("setAddColor", EVT_Void, partSetColorBias, EVT_Float, EVT_Float, EVT_Float),
    funcEntry3("setAddColorDist", EVT_Void, partSetColorBiasDist, EVT_Float, EVT_Float, EVT_Float),
    funcEntry1("setLighting", EVT_Void, partSetLighting, EVT_Int),
    funcEntry1("setIsWorldSpace", EVT_Void, partSetIsWorldspace, EVT_Int),
    funcEntry1("setVelocityInWorldSpace", EVT_Void, partSetVelocityInWorldSpace, EVT_Int),
    funcEntry1("setIllum", EVT_Void, partSetIllum, EVT_Float),
    funcEntry1("setIllumDist", EVT_Void, partSetIllumDist, EVT_Float),
    funcEntry1("setDeltaIllum", EVT_Void, partSetDeltaIllum, EVT_Float),
    funcEntry1("setDeltaIllumDist", EVT_Void, partSetDeltaIllumDist, EVT_Float),
    funcEntry1("setLifespan", EVT_Void, partSetLifespan, EVT_Float),
    funcEntry1("setLifespanDist", EVT_Void, partSetLifespanDist, EVT_Float),
    funcEntry1("setWaitSpan", EVT_Void, partSetWaitspan, EVT_Float),
    funcEntry1("setWaitSpanDist", EVT_Void, partSetWaitspanDist, EVT_Float),
    funcEntry1("setLength", EVT_Void, partSetLength, EVT_Float),
    funcEntry1("setLengthDist", EVT_Void, partSetLengthDist, EVT_Float),
    funcEntry1("setDeltaLength", EVT_Void, partSetDeltaLength, EVT_Float),
    funcEntry1("setDeltaLengthDist", EVT_Void, partSetDeltaLengthDist, EVT_Float),
    funcEntry1("setCircleSlices", EVT_Void, partSetSlices, EVT_Int),
    funcEntry3("setTumble", EVT_Void, partSetTumble, EVT_Float, EVT_Float, EVT_Float),
    funcEntry3("setDeltaTumble", EVT_Void, partSetDeltaTumble, EVT_Float, EVT_Float, EVT_Float),
    funcEntry3("setAnimation", EVT_Void, etgSetAnimation, EVT_ConstLabel, EVT_Float, EVT_Int),
    funcEntry3("setMeshAnimation", EVT_Void, etgSetMorphAnimation, EVT_ConstLabel, EVT_Float, EVT_Int),
    funcEntry1("setMeshStartFrame", EVT_Void, partSetMeshStartFrame, EVT_Float),
    funcEntry1("setFramerate", EVT_Void, partSetLoopFlag, EVT_Float),          //exe: partSetLoopFlag 0x004e3aa0
    funcEntry1("setLoopFlag", EVT_Void, partSetLoopFlag, EVT_Int),
    funcEntry1("setStartFrame", EVT_Void, partSetStartFrame, EVT_Int),
    funcEntry1("setAlphaMode", EVT_Void, partSetAlphaMode, EVT_Int),
    funcEntry1("setSpecular", EVT_Void, etgSetSpecular, EVT_Float),
    funcEntry1("setColorScheme", EVT_Void, partSetColorScheme, EVT_Int),
    funcEntry1("setTrueBillboard", EVT_Void, partSetTrueBillboard, EVT_Int),
    funcEntry1("setPseudoBillboard", EVT_Void, partSetPseudoBillboard, EVT_Int),
    funcEntryR1("setDepthWrite", EVT_Void, etgSetDepthWrite, EVT_Int, etgDepthWriteResolve),
    funcEntry1("setTurbulance", EVT_Void, cat_partSetTurbulance, EVT_Float),
    funcEntry1("setSegments", EVT_Void, cat_partSetSegments, EVT_Int),

    //modify a current particle system
    funcEntry2("modifyDrag", EVT_Void, etgModifyDrag, EVT_Int, EVT_Float),
    funcEntry2("modifyScale", EVT_Void, partModifyScale, EVT_Int, EVT_Float),
    funcEntry1("print", EVT_Void, cat_etgNullsub, EVT_Float),
    funcEntry2("modifyDeltaScale", EVT_Void, partModifyDeltaScale, EVT_Int, EVT_Float),
    funcEntry3("modifyDeltaLength", EVT_Void, partModifyDeltaLength, EVT_Int, EVT_Float, EVT_Int),
    funcEntry2("modifyColorA", EVT_Void, partModifyColorC, EVT_Int, EVT_RGBA),
    funcEntry2("modifyColor", EVT_Void, partModifyColorC, EVT_Int, EVT_RGB),
    funcEntry4("modifyColorBias", EVT_Void, partModifyColorBias, EVT_Int, EVT_Float, EVT_Float, EVT_Float),
    funcEntry2("modifyAddColor", EVT_Void, partModifyAddColor, EVT_Int, EVT_RGB),
    funcEntry5("modifyDeltaColor", EVT_Void, partModifyDeltaColor, EVT_Int, EVT_Float, EVT_Float, EVT_Float, EVT_Float),
    funcEntry2("modifyLighting", EVT_Void, partModifyLighting, EVT_Int, EVT_Float),
    funcEntry2("modifyIllum", EVT_Void, partModifyIllum, EVT_Int, EVT_Float),
    funcEntry2("modifyDeltaIllum", EVT_Void, partModifyDeltaIllum, EVT_Int, EVT_Float),
    funcEntry2("modifyLifespan", EVT_Void,  partModifyLifespan, EVT_Int, EVT_Float),
    funcEntry2("modifyAnimation", EVT_Void, partModifyAnimation, EVT_Int, EVT_ConstLabel),
    funcEntry2("modifyFramerate", EVT_Void, partModifyFramerate, EVT_Int, EVT_Float),
    funcEntry2("modifyVelLOF", EVT_Void, partModifyVelLOF, EVT_Int, EVT_Float),
    funcEntry2("modifyDeltaVelLOF", EVT_Void, partModifyDeltaVelLOF, EVT_Int, EVT_Float),
    funcEntry2("modifyVelR", EVT_Void, partModifyVelR, EVT_Int, EVT_Float),
    funcEntry2("modifyDeltaVelR", EVT_Void, partModifyDeltaVelR, EVT_Int, EVT_Float),
    funcEntry2("modifyMesh", EVT_Void, partModifyMesh, EVT_Int, EVT_Float),
    funcEntry2("modifyMeshAnimation", EVT_Void, partModifyMorph, EVT_Int, EVT_ConstLabel),
    funcEntry2("modifyMeshFramerate", EVT_Void, partModifyMorphFramerate, EVT_Int, EVT_Float),
    funcEntry2("modifyLength", EVT_Void, partModifyLength, EVT_Int, EVT_Float),
    funcEntry2("modifyAlphaMode", EVT_Void, partModifyAlphaMode, EVT_Int, EVT_Int),
    funcEntry2("modifySpecular", EVT_Void, partModifyExponent, EVT_Int, EVT_Float),
    funcEntry2("modifyColorScheme", EVT_Void, partModifyColorScheme, EVT_Int, EVT_Int),
    funcEntry2("modifyAlphaValue", EVT_Void, cat_partModifyAlphaValue, EVT_Int, EVT_Float),
    funcEntry2("modifyTrueBillboard", EVT_Void, partModifyPseudoBillboard, EVT_Int, EVT_Int),
    funcEntry2("modifyPseudoBillboard", EVT_Void, partModifyPseudoBillboard, EVT_Int, EVT_Int),
    funcEntry1("modifyBackgroundColor", EVT_Void, cat_etgNullsub, EVT_RGB),
    funcEntry2("modifyRotAngle", EVT_Void, cat_partModifyRotAngle, EVT_Int, EVT_Float),
    funcEntry2("modifyScaleX", EVT_Void, cat_partModifyScaleX, EVT_Int, EVT_Float),
    funcEntry2("modifyScaleY", EVT_Void, cat_partModifyScaleY, EVT_Int, EVT_Float),
    funcEntry4("modifyTumble", EVT_Void, partModifyTumble, EVT_Int, EVT_Float, EVT_Float, EVT_Float),

    //create particles
    funcEntryThisR2("createSprites", EVT_Int, etgCreateSprites, EVT_Int, EVT_Int, etgCreationResolve),
    funcEntryThisR2("createCircles", EVT_Int, etgCreateCircles, EVT_Int, EVT_Int, etgCreationResolve),
    funcEntryThisR2("createPoints", EVT_Int, etgCreatePoints, EVT_Int, EVT_Int, etgCreationResolve),
    funcEntryThisR2("createLines", EVT_Int, etgCreateLines, EVT_Int, EVT_Int, etgCreationResolve),
    funcEntryThisR2("createMeshes", EVT_Int, etgCreateMeshes, EVT_Int, EVT_Int, etgCreationResolve),
    funcEntryThisR2("createEBolts", EVT_Int, cat_etgCreateEBolts, EVT_Int, EVT_Int, etgCreationResolve),
    funcEntryThisR2("createLensFlares", EVT_Int, cat_etgCreateLensFlares, EVT_Int, EVT_Int, etgCreationResolve),

    //Basic mathematics.  Need more?
    funcEntry1("inc", EVT_Int, etgInc, EVT_Int),
    funcEntry1("dec", EVT_Int, etgDec, EVT_Int),
    funcEntry2("fdiv", EVT_Float, etgFdiv, EVT_Float, EVT_Float),
    funcEntry2("fmult", EVT_Float, etgFmult, EVT_Float, EVT_Float),
    funcEntry1("fdegsin", EVT_Float, cat_etgFdegsin, EVT_Float),
    funcEntry2("fadd", EVT_Float, etgFadd, EVT_Float, EVT_Float),
    funcEntry2("fsub", EVT_Float, etgFsub, EVT_Float, EVT_Float),
    funcEntry2("div", EVT_Int, etgDiv, EVT_Int, EVT_Int),
    funcEntry2("mult", EVT_Int, etgMult, EVT_Int, EVT_Int),
    funcEntry2("add", EVT_Int, etgAdd, EVT_Int, EVT_Int),
    funcEntry2("sub", EVT_Int, etgSub, EVT_Int, EVT_Int),
    funcEntry1("sin", EVT_Float, etgSin, EVT_Float),
    funcEntry1("cos", EVT_Float, etgCos, EVT_Float),
    funcEntry2("frandom", EVT_Float, etgFRandom, EVT_Float, EVT_Float),
    funcEntry2("irandom", EVT_Int, etgIRandom, EVT_Int, EVT_Int),
    funcEntry6("crandom", EVT_RGB, etgCRandom, EVT_Int, EVT_Int, EVT_Int, EVT_Int, EVT_Int, EVT_Int),
    funcEntry8("carandom", EVT_RGBA, etgCARandom, EVT_Int, EVT_Int, EVT_Int, EVT_Int, EVT_Int, EVT_Int, EVT_Int, EVT_Int),

    //special-case functions
    funcEntryThis0("deleteParentShip", EVT_Int, etgParentShipDelete),
    funcEntryThis0("hideParentShip", EVT_Int, etgParentShipHide),
    funcEntryThis0("omitParentShipFromRenderList", EVT_Int, cat_etgParentShipOmitFromRenderList),
    funcEntryThis1("playSound", EVT_Int, soundEffect, EVT_Int),
    funcEntryThis1("playChatter", EVT_Int, etgChatterEventPlay, EVT_Int),
    funcEntryThis0("deathCries", EVT_Int, etgDeathCriesPlay),
    funcEntryThis0("getColorScheme", EVT_Int, etgOwnerColorSchemeGet),
    funcEntryThis0("getEffectVelocity",   EVT_Int, etgEffectVelocityGet),
    funcEntryThis0("damageDone", EVT_Int, etgDamageDone),

    //adjust parameters of effects to be spawned
    funcEntry1("setEffOffsetLOF",   EVT_Void, etgEffOffsetLOF,  EVT_Float),
    funcEntry2("setEffChangeLOF",   EVT_Void, etgEffChangeLOF,  EVT_Float, EVT_Float),
    funcEntry2("setEffOffsetR",     EVT_Void, etgEffOffsetR,    EVT_Float, EVT_Float),
    funcEntry3("setEffOffsetXYZ",   EVT_Void, etgEffOffsetXYZ , EVT_Float, EVT_Float, EVT_Float),
    funcEntry1("setEffOffsetVelLOF",EVT_Void, etgEffOffsetVelLOF,EVT_Float),
    funcEntry1("setEffOffsetSpin",  EVT_Void, etgEffOffsetSpin, EVT_Float),
    funcEntry1("setEffOffsetTime",  EVT_Void, etgEffOffsetTime, EVT_Float),
    funcEntryThis1("setEffOffsetDrag",  EVT_Void, etgEffOffsetDrag, EVT_Float),
    funcEntry1("setEffDrag",        EVT_Void, etgEffDrag      , EVT_Float),
    funcEntry0("setEffDefaults",    EVT_Void, etgEffDefaults),
    funcEntry1("setEffAttachParent",EVT_Void, etgEffAttachParent, EVT_Int),
    funcEntry1("setEffSpin",EVT_Void, etgEffSpin, EVT_Int),
    funcEntry1("setEffDeltaSpin",EVT_Void, etgEffDeltaSpin, EVT_Int),

    //adjust parameters of current effect
    funcEntryThis3("effModifyXYZ",  EVT_Void,    etgThisOffsetXYZ, EVT_Float, EVT_Float, EVT_Float),
    funcEntryThis1("effScaleVelocity",  EVT_Void,etgThisScaleVelocity, EVT_Float),
    funcEntryThis1("effModifySpin",  EVT_Void,   etgThisOffsetSpin, EVT_Float),
    funcEntryThis1("effModifyDeltaSpin",  EVT_Void,   etgThisOffsetDeltaSpin, EVT_Float),
    funcEntryThis1("effModifyOffsetTime",  EVT_Void,   etgThisOffsetTime, EVT_Float),
    funcEntryThis1("effModifyDrag",  EVT_Void,      etgThisSetDrag, EVT_Float),

    //convert types
    funcEntry1("float2Int",  EVT_Int,    etgFloat2Int, EVT_Float),
    funcEntry1("int2Float",  EVT_Float,    etgInt2Float, EVT_Int),
    funcEntry3("int2Color",  EVT_RGB,    etgInts2Color, EVT_Int, EVT_Int, EVT_Int),
    funcEntry4("int2ColorA",  EVT_RGB,    etgInts2ColorA, EVT_Int, EVT_Int, EVT_Int, EVT_Int),
    funcEntry3("floats2Color",  EVT_RGB,    etgFloats2Color, EVT_Float, EVT_Float, EVT_Float),
    funcEntry4("floats2ColorA",  EVT_RGB,    etgFloats2ColorA, EVT_Float, EVT_Float, EVT_Float, EVT_Float),
    funcEntry1("colRed",  EVT_Float,    cat_etgColRed, EVT_RGBA),
    funcEntry1("colGreen",  EVT_Float,    cat_etgColGreen, EVT_RGBA),
    funcEntry1("colBlue",  EVT_Float,    cat_etgColBlue, EVT_RGBA),

    END_OP_FUNCTION_ENTRY,
};

//this table handles differing formats of the standard 'if' statement
etgcondentry etgConditionalTable[] =
{
    {"=",   "=",   {EOP_EqualVCI,        EOP_EqualVVI,        EOP_EqualVCF,        EOP_EqualVVF         }},
    {"==",  "=",   {EOP_EqualVCI,        EOP_EqualVVI,        EOP_EqualVCF,        EOP_EqualVVF         }},
    {"!=",  "!=",  {EOP_NotEqualVCI,     EOP_NotEqualVVI,     EOP_NotEqualVCF,     EOP_NotEqualVVF      }},
    {"<>",  "<>",  {EOP_NotEqualVCI,     EOP_NotEqualVVI,     EOP_NotEqualVCF,     EOP_NotEqualVVF      }},
    {">",   "<=",  {EOP_GreaterVCI,      EOP_GreaterVVI,      EOP_GreaterVCF,      EOP_GreaterVVF       }},
    {">=",  "<",   {EOP_GreaterEqualVCI, EOP_GreaterEqualVVI, EOP_GreaterEqualVCF, EOP_GreaterEqualVVF  }},
    {"<",   ">=",  {EOP_LessVCI,         EOP_LessVVI,         EOP_LessVCF,         EOP_LessVVF          }},
    {"<=",  ">",   {EOP_LessEqualVCI,    EOP_LessEqualVVI,    EOP_LessEqualVCF,    EOP_LessEqualVVF     }},
    {"end", "end", {EOP_Nop,             EOP_Nop,             EOP_Nop,             EOP_Nop              }}
};

//table for parsing the etg script
etgeffectstatic *etgEffectLoad(char *directory,char *field,void *dataToFillIn);
void etgEffectLoadVoid(char *directory,char *field,void *dataToFillIn);
void etgEffectTestKey(char *directory,char *field,void *dataToFillIn);
void etgEffectProfileKey(char *directory,char *field,void *dataToFillIn);
void etgDeathEventSet(char *directory,char *field,void *dataToFillIn);
void etgFireEventSet(char *directory,char *field,void *dataToFillIn);
void etgHitEventSet(char *directory,char *field,void *dataToFillIn);
void etgBulletEffectSet(char *directory,char *field,void *dataToFillIn);
//void etgBulletDeflectEffectSet(char *directory,char *field,void *dataToFillIn);
void etgBulletDestroyEffectSet(char *directory,char *field,void *dataToFillIn);
void etgResourceEffectSet(char *directory,char *field,void *dataToFillIn);
void etgTractorBeamEffectSet(char *directory,char *field,void *dataToFillIn);
void etgSpecialPurposeEffectSet(char *directory,char *field, void *dataToFillIn);
void etgBulletColorSet(char *directory,char *field,void *dataToFillIn);
void etgSetBigDeathFactor(char *directory,char *field,void *dataToFillIn);
void etgDamageEventSet(char *directory,char *field,void *dataToFillIn);
void etgHyperspaceEventSet(char *directory,char *field,void *dataToFillIn);
void etgNumberEffectsParse(char *directory,char *field,void *dataToFillIn);
void etgSoftwareEffect(char *directory,char *field,void *dataToFillIn);
scriptEntry etgScriptParseTable[] =
{
    {"LoadEffect",                  etgEffectLoadVoid, NULL},
    {"SoftwareVersion",             etgSoftwareEffect, NULL},
    {"setDeathEvent",               etgDeathEventSet, NULL},
    {"setFireEvent",                etgFireEventSet, NULL},
    {"setHitEvent",                 etgHitEventSet, NULL},
    {"setBulletDestroyEvent",       etgBulletDestroyEffectSet, NULL},
//    {"setBulletDeflectEvent",       etgBulletDeflectEffectSet, NULL},
    {"bigBoomFactor",               etgSetBigDeathFactor, NULL},
    {"setBulletEffect",             etgBulletEffectSet, NULL},
    {"setBulletColor",              etgBulletColorSet, NULL},
    {"setAsteroidEffect",           etgResourceEffectSet, NULL},
    {"setGaseousEffect",            etgResourceEffectSet, (void *)1},
    {"setTractorBeamEffect",        etgTractorBeamEffectSet, NULL},
    {"setSpecialPurposeEffect",     etgSpecialPurposeEffectSet, NULL},
    {"setDamageEvent",              etgDamageEventSet, NULL},
    {"setHyperspaceEvent",          etgHyperspaceEventSet, NULL},
    {"NumberEffects",               etgNumberEffectsParse, NULL},
    {"etgHistoryScalarMin",         scriptSetUdwordCB, &etgHistoryScalarMin},  // CAT: the exe's table uses scriptSetUdwordCB (0x0087ac00)
    {"etgSoftwareScalarDamage",     scriptSetReal32CB, &etgSoftwareScalarDamage},
    {"etgSoftwareScalarHit",        scriptSetReal32CB, &etgSoftwareScalarHit},
    {"etgSoftwareScalarFire",       scriptSetReal32CB, &etgSoftwareScalarFire},
#if ETG_TESTING
    {"TestEffect",                  etgEffectTestKey, NULL},
    {"ProfileEffect",               etgEffectProfileKey, NULL},
#endif
    {NULL, NULL, NULL}
};
sdword etgNumberEffectsExpected = -1;
sdword etgNumberEffectsLoaded = 0;
//list of script files to load
// CAT: the exe's list (0x0087ac40): general.script, then one script per Cataclysm race (etg\race_*.script)
char *etgScriptFilename[] =
{
    "general.script",
    "RACE_SECT.script",
    "RACE_BEAST.script",
    "RACE_KUSHAN.script",
    "RACE_TAIDAN.script",
    "RACE_P1.script",
    "RACE_TRADERS.script",
    "RACE_DERELICT.script",
    NULL
};
#if ETG_TESTING
//stuff for testing effects
struct
{
    char *type;                                 //type of light
    char *lightName;                            //name of light
    char *effectName;                           //name of effect
    regionhandle region;                        //hot key
    sdword nTimes;                              //number of times to execute
    sdword iTime;                               //updated as the effect is being profiled
    real32 duration;                            //duration over which to execute these effects
}
etgTestKey[ETG_NumberTestKeys];
sdword etgTestKeyIndex;
#endif //ETG_TESTING

//dispatch tables for standard events
//etgeffectstatic *etgDefaultBoom;
//etgeffectstatic *etgDefaultBlast;
etglod *etgDeathEventTable[NUM_RACES][NUM_CLASSES][EDT_NumberExplosionTypes];
etglod *etgDeathEventTableDerelict[NUM_DERELICTTYPES][EDT_NumberExplosionTypes];
etglod *etgGunEventTable[NUM_RACES][NUM_GUN_SOUND_TYPES][EGT_NumberGunTypes];
etglod *etgResourceEffectTable[NUM_RACES][ETG_NumberResourceTypes];
etglod *etgTractorBeamEffectTable[NUM_RACES];
etglod *etgSpecialPurposeEffectTable[EGT_NumberOfSpecialEffects];
etglod *etgDamageEffectTable[NUM_CLASSES][DMG_NumberDamageTypes];
etglod *etgHyperspaceEffect = NULL;

sdword etgBigDeathFactor[NUM_RACES][NUM_CLASSES];
sdword etgBigDeathFactorDerelict[NUM_DERELICTTYPES];
color etgBulletColor[NUM_RACES][NUM_GUN_SOUND_TYPES];
ubyte etgDeathModeByGunType[] =
{
    /*GS_LargeEnergyCannon,    */EDT_ProjectileHit,
    /*GS_LargeIonCannon,       */EDT_BeamHit,
    /*GS_LargePlasmaBomb,      */EDT_ProjectileHit,
    /*GS_LargeProjectile,      */EDT_ProjectileHit,
    /*GS_MediumEnergyCannon,   */EDT_ProjectileHit,
    /*GS_MediumIonCannon,      */EDT_BeamHit,
    /*GS_MediumPlasmaBomb,     */EDT_ProjectileHit,
    /*GS_MediumProjectile,     */EDT_ProjectileHit,
    /*GS_MineLauncher,         */EDT_ProjectileHit,
    /*GS_MissleLauncher,       */EDT_ProjectileHit,
    /*GS_SmallEnergyCannon,    */EDT_ProjectileHit,
    /*GS_SmallIonCannon,       */EDT_ProjectileHit,
    /*GS_SmallPlasmaBomb,      */EDT_ProjectileHit,
    /*GS_SmallProjectile,      */EDT_ProjectileHit,
    /*GS_VeryLargeEnergyCannon,*/EDT_ProjectileHit,
    /*GS_VeryLargeIonCannon,   */EDT_ProjectileHit,
    /*GS_VeryLargePlasmaBomb,  */EDT_ProjectileHit,
    /*GS_Laser,                */EDT_BeamHit,
};

//spacial offset information for effects to be created in subsequent 'spawn' calls
struct
{
    real32 offsetLOF;
    real32 offsetR, offsetTheta;
    real32 changeLOFTheta, changeLOFMu;
    vector offsetXYZ;
    real32 offsetVelLOF;
    real32 offsetSpin;
    real32 offsetTime;
    real32 drag;
    bool32   attachParent;
    real32 spin;
    real32 deltaSpin;
}
etgEffOffset =
{
    0.0f,
    0.0f, 0.0f,
    0.0f, 0.0f,
    {0.0f, 0.0f, 0.0f},
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    FALSE,
    0.0f,
    0.0f
};

//ETG mesh registry
//meshdata *etgMeshRegistry[ETG_NumberMeshes];
//sdword etgMeshRegistryIndex = 0;

typedef struct
{
    meshdata *mesh;                             //NULL = registry slot free
    char *filename;
}
etgmeshreg;
etgmeshreg etgMeshRegistry[ETG_NumberMeshes];

#if ETG_DISABLEABLE
sdword etgEffectsEnabled = TRUE;
#endif

//variables for scanning time index blocks
etgvarentry *etgTimeIndexVar;
real32 etgTimeIndexTime;

//variables for ETG user tweaks:
real32 etgSoftwareScalarDamage = ETG_SoftwareScalarDamage;//scale damage effects down when in software mode.
real32 etgSoftwareScalarHit = ETG_SoftwareScalarHit;//scale down hits, deflections, bullet deaths
real32 etgSoftwareScalarFire = ETG_SoftwareScalarFire;//scale down muzzle flash effects
sdword etgHistoryScalar = 256;                  //scale down the number of effects at the user's command
sdword etgHistoryScalarMin = ETG_HistoryScalarMin;//minimum frequency scale-down
bool32   etgDamageEffectsEnabled = TRUE;          //damage effects on
bool32   etgHitEffectsEnabled = TRUE;             //hit effects on
bool32   etgFireEffectsEnabled = TRUE;            //muzzle flash effects on
bool32   etgBulletEffectsEnabled = TRUE;          //bullet effects on

/*=============================================================================
    Functions:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : etgLODLoad
    Description : Loads in an effect into a LOD structure, growing or creating
                    the structure as needed.
    Inputs      : directory,name - full filename of effect to load (when combined).
                  oldLOD - LOD structure to grow or NULL if not already loaded.
    Outputs     : allocates and returns a LOD structure of oldLOD is NULL, or
                    re-allocates and grows the structure for returning.
    Return      : newly (re)allocated structure.
----------------------------------------------------------------------------*/
etglod *etgLODLoad(char *directory, char *name, etglod *oldLOD)
{
    char path[PATH_MAX];
    etglod *newLOD;

    //create path to effect
    strcpy(path, directory);
    strcat(path, name);

    //allocate a LOD structure as needed
    if (oldLOD == NULL)
    {
        newLOD = memAlloc(etgLODSize(1), "NewEtgLod", NonVolatile);
        newLOD->nLevels = 0;
    }
    else
    {
        newLOD = memAlloc(etgLODSize(oldLOD->nLevels + 1), "Realloc'dEtgLod", NonVolatile)
        memcpy(newLOD, oldLOD, etgLODSize(oldLOD->nLevels));
        memFree(oldLOD);
    }
    //load in the effect
    etgErrorRecoverable = FALSE;
    newLOD->level[newLOD->nLevels] = etgEffectCodeLoad(path);
    newLOD->nLevels++;                                      //count the new level loaded
    return(newLOD);
}

/*-----------------------------------------------------------------------------
    Name        : etgEffectLoad
    Description : Load in an effect after manipulating the filename a bit.
    Inputs      : Standard scripting parameters.
    Outputs     : Newly loaded effect.
    Return      :
    Note        : Can be called from script loading stuff or directly from
                    special loading functions.
----------------------------------------------------------------------------*/
void etgEffectLoadVoid(char *directory,char *field,void *dataToFillIn)
{
    etgEffectLoad(directory, field, dataToFillIn);
    return;
}

etgeffectstatic *etgEffectLoad(char *directory,char *field,void *dataToFillIn)
{
    char path[PATH_MAX];

    strcpy(path, directory);
    strcat(path, field);
    etgErrorRecoverable = FALSE;
    return(etgEffectCodeLoad(path));
}

/*-----------------------------------------------------------------------------
    Name        : etgSoftwareEffect
    Description : Script-parsing callback to define what effect to use for a
                    specified effect if playing in software
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void etgSoftwareEffect(char *directory,char *field,void *dataToFillIn)
{
    etgeffectstatic *hardwareEffect;
    char *hardwareName, *softwareName;

    hardwareName = strtok(field, ETG_TokenDelimiters);
    softwareName = strtok(NULL, ETG_TokenDelimiters);
    hardwareEffect = etgEffectLoad(directory, hardwareName, NULL);
    hardwareEffect->softwareVersion = etgEffectLoad(directory, softwareName, NULL);
}

/*-----------------------------------------------------------------------------
    Name        : etgSetBigDeathFactor
    Description : Set the factor at which a ship dies cataclysmically
    Inputs      : field in the format of <shipClass>, <number>
                    number is a negative, absolute number
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void etgSetBigDeathFactor(char *directory,char *field,void *dataToFillIn)
{
    char *param;
    ShipClass classy;
    ShipRace racerX;
    DerelictType typist;
    sdword *whereToScan;
    sdword nScanned;

    param = strtok(field, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    racerX = StrToShipRace(field);

    if (racerX == -1)
    {                                                       //if we could not parse a race, it may be a derelict type
        typist = StrToDerelictType(param);
        dbgAssertOrIgnore(typist >= 0 && typist < NUM_DERELICTTYPES);
        whereToScan = &etgBigDeathFactorDerelict[typist];
        goto scanTheFactor;
    }

    dbgAssertOrIgnore(racerX >= 0 && racerX < NUM_RACES);
    param = strtok(NULL, ETG_TokenDelimiters);                           //get the second parameter (class)
    dbgAssertOrIgnore(param);
    classy = StrToShipClass(param);
    dbgAssertOrIgnore(classy >= CLASS_Mothership && classy < NUM_CLASSES);
    whereToScan = &etgBigDeathFactor[racerX][classy];

scanTheFactor:
    param = strtok(NULL, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    nScanned = sscanf(param, "%d", whereToScan);
    dbgAssertOrIgnore(nScanned == 1);
}

/*-----------------------------------------------------------------------------
    Name        : etgNumberEffectsParse
    Description : Read in the number of effects to expect, for the horse race.
    Inputs      : standard script-parsing fare
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void etgNumberEffectsParse(char *directory,char *field,void *dataToFillIn)
{
    // CAT: scriptSetUdwordCB and no range assert (etgNumberEffectsParse 0x004691c0)
    scriptSetUdwordCB(directory, field, &etgNumberEffectsExpected);
    HorseRaceBeginBar(ETG_BAR);
}

/*-----------------------------------------------------------------------------
    Name        : etgTokTheType
    Description : Called by the following function, calls strtok to get a
                    explosion type.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
sdword etgTokTheType(void)
{
    char *param;
    sdword exType;

    param = strtok(NULL, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    if (strcasecmp(param, "damage") == 0)          // CAT: the exe compares these keywords with _stricmp (etgDeathEventSet 0x004691e0, etgDamageEventSet 0x00469d90, the *EventSet "NULL" tests)
    {
        exType = EDT_AccumDamage;
    }
    else if (strcasecmp(param, "projectile") == 0)
    {
        exType = EDT_ProjectileHit;
    }
    else if (strcasecmp(param, "beam") == 0)
    {
        exType = EDT_BeamHit;
    }
    else if (strcasecmp(param, "collision") == 0)
    {
        exType = EDT_Collision;
    }
    else if (strcasecmp(param, "special") == 0)
    {
        exType = EDT_Special;
    }
    else
    {
        dbgFatalf(DBG_Loc, "Unrecognised explosion type '%s'", param);
    }
    return(exType);
}

/*-----------------------------------------------------------------------------
    Name        : etgDeathEventSet
    Description : Sets a death event for a ship.  It is assumed that all death
                    events are stubbed out already.
    Inputs      : scripting callback
                  field should have the format: <className>, <deathType>, <eventFile>
    Outputs     : loads in the named effect for the named death type of the named ship class
    Return      :
----------------------------------------------------------------------------*/
void etgDeathEventSet(char *directory,char *field,void *dataToFillIn)
{
    char *param;
    ShipClass classy;
    ShipRace racerX;
    DerelictType typist;
    sdword exType;
    etglod **whereToLoad;

    param = strtok(field, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    racerX = StrToShipRace(field);

    if (racerX == -1)
    {                                                       //if we could not parse a race, it may be a derelict type
        typist = StrToDerelictType(param);
        dbgAssertOrIgnore(typist >= 0 && typist < NUM_DERELICTTYPES);
        exType = etgTokTheType();
        whereToLoad = &etgDeathEventTableDerelict[typist][exType];
        goto loadTheEffect;
    }

    dbgAssertOrIgnore(racerX >= 0 && racerX < NUM_RACES);
    param = strtok(NULL, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    classy = StrToShipClass(param);
    dbgAssertOrIgnore(classy >= CLASS_Mothership && classy < NUM_CLASSES);

    exType = etgTokTheType();
    whereToLoad = &etgDeathEventTable[racerX][classy][exType];

loadTheEffect:
    param = strtok(NULL, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    if (strcasecmp(param, "NULL") == 0)
    {
        //!!! notice the potential memory leak here?  There could be valid effects assigned to this pointer and then this pointer is set to NULL.
        *whereToLoad = NULL;
    }
    else
    {
        *whereToLoad = etgLODLoad(directory, param, *whereToLoad);
    }
}

/*-----------------------------------------------------------------------------
    Name        : etgParseRaceSoundParam
    Description : Parse a race, gun sound type and parameter from a string
    Inputs      : params - string to parse
    Outputs     : race - race parsed from string
                  gunSound - sound parsed from string
                  param - remainder of string
    Return      : void
----------------------------------------------------------------------------*/
void etgParseRaceSoundParam(char *params, ShipRace *race, GunSoundType *gunSound, char **param)
{
    *param = strtok(params, ETG_TokenDelimiters);
    dbgAssertOrIgnore(*param);
    *race = StrToShipRace(params);
    dbgAssertOrIgnore(*race >= 0 && *race < NUM_RACES);

    *param = strtok(NULL, ETG_TokenDelimiters);
    dbgAssertOrIgnore(*param);
    *gunSound = StrToGunSoundType(*param);
    dbgAssertOrIgnore(*gunSound >= GS_LargeEnergyCannon && *gunSound < NUM_GUN_SOUND_TYPES);

    *param = strtok(NULL, ETG_TokenDelimiters);
    dbgAssertOrIgnore(*param);
}

/*-----------------------------------------------------------------------------
    Name        : etgFireEventSet
    Description : Same as before, just for guns.  This one defines the shot event.
    Inputs      :
    Outputs     : Sets the fire, hit or bullet effects.
    Return      :
----------------------------------------------------------------------------*/
void etgFireEventSet(char *directory,char *field,void *dataToFillIn)
{
    char *param;
    ShipRace racerX;
    GunSoundType gunSound;

    etgParseRaceSoundParam(field, &racerX, &gunSound, &param);

    if (strcasecmp(param, "NULL") == 0)
    {
        etgGunEventTable[racerX][gunSound][EGT_GunFire] = NULL;
    }
    else
    {
        etgGunEventTable[racerX][gunSound][EGT_GunFire] = etgLODLoad(directory, param, etgGunEventTable[racerX][gunSound][EGT_GunFire]);
    }
}

void etgHitEventSet(char *directory,char *field,void *dataToFillIn)
{
    char *param;
    ShipRace racerX;
    GunSoundType gunSound;

    etgParseRaceSoundParam(field, &racerX, &gunSound, &param);

    if (strcasecmp(param, "NULL") == 0)
    {
        etgGunEventTable[racerX][gunSound][EGT_GunFire] = NULL;
    }
    else
    {
        etgGunEventTable[racerX][gunSound][EGT_GunHit] = etgLODLoad(directory, param, etgGunEventTable[racerX][gunSound][EGT_GunHit]);
    }
}

void etgBulletEffectSet(char *directory,char *field,void *dataToFillIn)
{
    char *param;
    ShipRace racerX;
    GunSoundType gunSound;

    etgParseRaceSoundParam(field, &racerX, &gunSound, &param);

    if (strcasecmp(param, "NULL") == 0)
    {
        etgGunEventTable[racerX][gunSound][EGT_GunBullet] = NULL;
    }
    else
    {
        etgGunEventTable[racerX][gunSound][EGT_GunBullet] = etgLODLoad(directory, param, etgGunEventTable[racerX][gunSound][EGT_GunBullet]);
    }
}

void etgBulletDestroyEffectSet(char *directory,char *field,void *dataToFillIn)
{
    char *param;
    ShipRace racerX;
    GunSoundType gunSound;

    etgParseRaceSoundParam(field, &racerX, &gunSound, &param);

    if (strcasecmp(param, "NULL") == 0)
    {
        etgGunEventTable[racerX][gunSound][EGT_BulletDestroyed] = NULL;
    }
    else
    {
        etgGunEventTable[racerX][gunSound][EGT_BulletDestroyed] = etgLODLoad(directory, param, etgGunEventTable[racerX][gunSound][EGT_BulletDestroyed]);
    }
}
/*
void etgBulletDeflectEffectSet(char *directory,char *field,void *dataToFillIn)
{
    char *param;
    ShipRace racerX;
    GunSoundType gunSound;

    etgParseRaceSoundParam(field, &racerX, &gunSound, &param);

    if (strcasecmp(param, "NULL") == 0)
    {
        etgGunEventTable[racerX][gunSound][EGT_BulletDeflected] = NULL;
    }
    else
    {
        etgGunEventTable[racerX][gunSound][EGT_BulletDeflected] = etgLODLoad(directory, param, etgGunEventTable[racerX][gunSound][EGT_BulletDeflected]);
    }
}
*/
void etgResourceEffectSet(char *directory,char *field,void *dataToFillIn)
{
    char *param;
    ShipRace racerX;

    param = strtok(field, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    racerX = StrToShipRace(field);
    dbgAssertOrIgnore(racerX >= 0 && racerX < NUM_RACES);

    param = strtok(NULL, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    if (dataToFillIn == NULL)
    {                                                       //NULL = asteroid, !NULL = gaseous
        etgResourceEffectTable[racerX][ETG_AsteroidEffect] =
            etgLODLoad(directory, param, etgResourceEffectTable[racerX][ETG_AsteroidEffect]);
    }
    else
    {
        etgResourceEffectTable[racerX][ETG_GaseousEffect] =
            etgLODLoad(directory, param, etgResourceEffectTable[racerX][ETG_GaseousEffect]);
    }
}

void etgTractorBeamEffectSet(char *directory,char *field,void *dataToFillIn)
{
    char *param;
    ShipRace racerX;

    param = strtok(field, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    racerX = StrToShipRace(field);
    dbgAssertOrIgnore(racerX >= 0 && racerX < NUM_RACES);

    param = strtok(NULL, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    etgTractorBeamEffectTable[racerX] = etgLODLoad(directory, param, etgTractorBeamEffectTable[racerX]);
}

void etgDamageEventSet(char *directory,char *field,void *dataToFillIn)
{
    char *param;
    ShipClass classy;
    sdword exType;

    param = strtok(field, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    classy = StrToShipClass(field);
    dbgAssertOrIgnore(classy >= CLASS_Mothership && classy < NUM_CLASSES);

    param = strtok(NULL, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    if (strcasecmp(param, "light") == 0)
    {
        exType = DMG_Light;
    }
    else if (strcasecmp(param, "heavy") == 0)
    {
        exType = DMG_Heavy;
    }
    else if (strcasecmp(param, "dying") == 0)
    {
        exType = DMG_Dying;
    }
    else
    {
        dbgFatalf(DBG_Loc, "Unrecognized damage type '%s'", param);
    }

    param = strtok(NULL, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    if (strcasecmp(param, "NULL") == 0)
    {
        etgDamageEffectTable[classy][exType] = NULL;
    }
    else
    {
        etgDamageEffectTable[classy][exType] = etgLODLoad(directory, param, etgDamageEffectTable[classy][exType]);
    }
}

void etgHyperspaceEventSet(char *directory,char *field,void *dataToFillIn)
{
    char *param;

    param = strtok(field, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    if (strcasecmp(param, "NULL") == 0)
    {
        etgHyperspaceEffect = NULL;
    }
    else
    {
        etgHyperspaceEffect = etgLODLoad(directory, param, etgHyperspaceEffect);
    }
}

void etgSpecialPurposeEffectSet(char *directory,char *field,void *dataToFillIn)
{
    char *param;
    SpecialEffectType effectnum;

    param = strtok(field, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    effectnum = StrToEffectNum(field);
    dbgAssertOrIgnore(effectnum >= 0 && effectnum < EGT_NumberOfSpecialEffects);

    param = strtok(NULL, ETG_TokenDelimiters);
    dbgAssertOrIgnore(param);
    etgSpecialPurposeEffectTable[effectnum] = etgLODLoad(directory, param, etgSpecialPurposeEffectTable[effectnum]);
}


void etgBulletColorSet(char *directory,char *field,void *dataToFillIn)
{
    char *param;
    ShipRace racerX;
    GunSoundType gunSound;
    udword red, green, blue, nScanned;

    etgParseRaceSoundParam(field, &racerX, &gunSound, &param);

    nScanned = sscanf(param, "%d", &red);
    dbgAssertOrIgnore(nScanned == 1);
    dbgAssertOrIgnore(red >= 0 && red < 256);
    param = strtok(NULL, ETG_TokenDelimiters);
    nScanned = sscanf(param, "%d", &green);
    dbgAssertOrIgnore(nScanned == 1);
    dbgAssertOrIgnore(green >= 0 && green < 256);
    param = strtok(NULL, ETG_TokenDelimiters);
    nScanned = sscanf(param, "%d", &blue);
    dbgAssertOrIgnore(nScanned == 1);
    dbgAssertOrIgnore(blue >= 0 && blue < 256);
    etgBulletColor[racerX][gunSound] = colRGB(red, green, blue);
}

#if ETG_TESTING
/*-----------------------------------------------------------------------------
    Name        : etgEffectTest
    Description : key-region callback handler for testing effects
    Inputs      : ID - index into etgTestKey of effect to test
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
udword etgEffectTest(regionhandle reg, sdword ID, udword event, udword data)
{
    etgeffectstatic *stat;
    Effect *newEffect;
    MEXChunk *mexChunk;
    vector *posChunk, *LOFChunk, LOFTemp, LOF;//, position;
    Ship *ship;
    ShipStaticInfo *shipStatic;
    matrix coordSystem;
    sdword size;

    if (multiPlayerGame)
    {
#if ETG_VERBOSE_LEVEL >= 1
        dbgMessagef("Effect testing disabled in multiplayer game");
#endif
        return(0);
    }

    //get ship and ship static pointers
    if (selSelected.numShips == 0)
    {
#if ETG_VERBOSE_LEVEL >= 1
        dbgMessagef("Select a ship before trying to test an effect");
#endif
        return(0);
    }
    ship = selSelected.ShipPtr[0];
    shipStatic = (ShipStaticInfo *)ship->staticinfo;

    //find the light
    mexChunk = mexGetChunk(shipStatic->common.pMexData, etgTestKey[ID].type, etgTestKey[ID].lightName);
    if (mexChunk == NULL)
    {
#if ETG_VERBOSE_LEVEL >= 1
        dbgMessagef("Light '%s' type '%s' not found in ship 0x%x", etgTestKey[ID].lightName, etgTestKey[ID].type, ship);
#endif
        return(0);
    }
    if ((strcmp(mexChunk->type, "Eng") == 0) || (strcmp(mexChunk->type, "Gun") == 0))
    {
        LOFChunk = &((MEXEngineChunk *)mexChunk)->normal;
        posChunk = &((MEXEngineChunk *)mexChunk)->position;
    }
    else
    {
#if ETG_VERBOSE_LEVEL >= 1
        dbgMessagef("Light '%s' type '%s' not a gun or engine chunk.", etgTestKey[ID].lightName, etgTestKey[ID].type);
#endif
        return(0);
    }

    //find the effect
    stat = etgEffectStaticFind(etgTestKey[ID].effectName, FALSE);
    if (stat == NULL)
    {
#if ETG_VERBOSE_LEVEL >= 1
        dbgMessagef("Effect '%s' not found.", etgTestKey[ID].effectName);
#endif
        return(0);
    }
#if ETG_VERBOSE_LEVEL >= 1
    dbgMessagef("Testing effect '%s' with light '%s'", etgTestKey[ID].effectName, etgTestKey[ID].lightName);
#endif

    //now we've found everything we need, let's make a matrix
    mexVecToVec(&LOFTemp, LOFChunk);
    matMultiplyMatByVec(&LOF, &ship->rotatable.rotinfo.coordsys, &LOFTemp);//LOF in world coords
    matCreateCoordSysFromHeading(&coordSystem, &LOF);
    //now we have a coordinate system, let's create an effect
//    size = etgEffectSize(stat->nParticleBlocks);            //compute size of effect
    size = stat->effectSize;
    newEffect = memAlloc(size, "TestingEffect", 0);         //allocate the new effect
    newEffect->header.objtype = OBJ_EffectType;
    newEffect->obj.catMeshState = NULL;                     // CAT: the SpaceObj +0x80/+0x84 fields (debug creator: no
    newEffect->obj.catEffectBlock = NULL;                   //      block, the free paths accept NULL)
    newEffect->header.flags = SOF_Rotatable | SOF_AttachPosition | SOF_AttachVelocity | SOF_AttachCoordsys;
    newEffect->staticinfo = stat;
    ClearNode(newEffect->obj.renderlink);
    newEffect->obj.currentLOD = ship->obj.currentLOD;
    newEffect->obj.cameraDistanceSquared = ship->obj.cameraDistanceSquared;
    newEffect->timeElapsed = 0.0f;                          //brand new !!! how about time-offsets?

//    vecAdd(newEffect->obj.posinfo.position, position, ship->obj.posinfo.position);
    newEffect->obj.posinfo.position = ship->obj.posinfo.position;
    vecAddToScalarMultiply(newEffect->obj.posinfo.position, ship->obj.posinfo.velocity, universe.phystimeelapsed);
    etgEffectCodeStart(stat, newEffect, 0, NULL);           //get the code a-runnin' (CAT: parameter array)
    if (bitTest(stat->specialOps, ESO_SortForward))
    {                                                       //make it sort forward, if applicable
        bitSet(newEffect->header.flags, SOF_AlwaysSortFront);
    }
    SET_MOVING_IMMOBILE(newEffect->obj.posinfo.isMoving);
    newEffect->obj.posinfo.haventCalculatedDist = TRUE;
    newEffect->rotatable.rotinfo.coordsys = coordSystem;
    univUpdateObjRotInfo((SpaceObjRot *)newEffect);

    newEffect->obj.posinfo.velocity = ship->obj.posinfo.velocity;
    newEffect->drag = 1.0f;                                 //default is no drag at all
//    newEffect->owner = ship;                                // ship that hosts this effect
    newEffect->owner = NULL;
    newEffect->nSpawnedAttachedToOwner = 0;
    newEffect->magnitudeSquared = 1.0f;
    listAddNode(&universe.SpaceObjList,&(newEffect->header.objlink),newEffect);
    listAddNode(&universe.effectList,&(newEffect->effectLink),newEffect);
    if (bitTest(stat->specialOps, ESO_WorldRender))
    {
        bitSet(newEffect->header.flags, SOF_Hide);
        univAddToWorldList((Derelict *)newEffect);
    }
    else
    {
        univAddObjToRenderListIf((SpaceObj *)newEffect,(SpaceObj *)ship);     // add to render list if parent ship is in render list
    }

    return(0);
}

/*-----------------------------------------------------------------------------
    Name        : etgEffectProfileBaby
    Description : Baby callback for profiling an effect
    Inputs      : ID - index of test key structure
                  data -  NULL
    Outputs     :
    Return      : TRUE if we've profiled enough already
----------------------------------------------------------------------------*/
bool32 etgEffectProfileBaby(udword ID, void *data, struct BabyCallBack *baby)
{
    if (etgTestKey[ID].iTime <= 0)
    {
        return(TRUE);
    }

    etgEffectTest(NULL, ID, 0xffffffff, 0);                 //test an effect
    etgTestKey[ID].iTime--;                                 //decrement the counter
    if (etgTestKey[ID].iTime == 0)                          //see if it's timed out
    {
        return(TRUE);
    }
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : etgEffectProfile
    Description : Profile and effect (region callback)
    Inputs      : standard region crap
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
udword etgEffectProfile(regionhandle reg, sdword ID, udword event, udword data)
{
    if (etgTestKey[ID].iTime != 0)
    {                                                       //if already being profiled
        etgTestKey[ID].iTime = 0;                           //stop the profile
        return(0);
    }
    //start the baby calling
    taskCallBackRegister(etgEffectProfileBaby, ID, NULL, min(etgTestKey[ID].duration / (real32)etgTestKey[ID].nTimes, UNIVERSE_UPDATE_PERIOD));

    etgTestKey[ID].iTime = etgTestKey[ID].nTimes - 1;
    return(0);
}

/*-----------------------------------------------------------------------------
    Name        : etgEffectTestKey
    Description : Script callback for testing an effect
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void etgEffectTestKey(char *directory,char *field,void *dataToFillIn)
{
    keyindex key;
    char *lightName, *effectName, *type;

    //find key number
    lightName = strtok(field, ETG_TokenDelimiters);
#if ETG_ERROR_CHECKING
    if (lightName == NULL)
    {
        dbgFatalf(DBG_Loc, "Error in %s\\%s: TestEffect format is '<keyChar>, <LightName>, <effectName>."ETG_Directory, ETG_Script);
    }
#endif
    key = *lightName;
    //find mex type
    type = strtok(NULL, ETG_TokenDelimiters);
#if ETG_ERROR_CHECKING
    if (type == NULL)
    {
        dbgFatalf(DBG_Loc, "Error in %s\\%s: TestEffect format is '<keyChar>, <LightName>, <effectName>."ETG_Directory, ETG_Script);
    }
#endif
    //find gunName
    lightName = strtok(NULL, ETG_TokenDelimiters);
#if ETG_ERROR_CHECKING
    if (lightName == NULL)
    {
        dbgFatalf(DBG_Loc, "Error in %s\\%s: TestEffect format is '<keyChar>, <LightName>, <effectName>."ETG_Directory, ETG_Script);
    }
#endif
    //find effect name
    effectName = strtok(NULL, ETG_TokenDelimiters);
#if ETG_ERROR_CHECKING
    if (effectName == NULL)
    {
        dbgFatalf(DBG_Loc, "Error in %s\\%s: TestEffect format is '<keyChar>, <LightName>, <effectName>."ETG_Directory, ETG_Script);
    }
#endif

    //now create a key region to execute this fandangled effect
#if ETG_ERROR_CHECKING
    if (etgTestKeyIndex >= ETG_NumberTestKeys)
    {
        dbgFatalf(DBG_Loc, "Exceeded %d test keys.", ETG_NumberTestKeys);
    }
#endif
#if ETG_VERBOSE_LEVEL >= 1
    dbgMessagef("Adding effect test key '%c' to test effect '%s' with vector from '%s'", key, effectName, lightName);
#endif
    etgTestKey[etgTestKeyIndex].type = memStringDupeNV(type);
    etgTestKey[etgTestKeyIndex].lightName = memStringDupeNV(lightName);
    etgTestKey[etgTestKeyIndex].effectName = memStringDupeNV(effectName);
    etgTestKey[etgTestKeyIndex].region = regKeyChildAlloc(ghMainRegion, etgTestKeyIndex, RPE_KeyDown, (regionfunction) etgEffectTest, 1, key);
    etgTestKey[etgTestKeyIndex].nTimes = 1;
    etgTestKey[etgTestKeyIndex].iTime = 0;
    etgTestKey[etgTestKeyIndex].duration = 1.0f;
    etgTestKeyIndex++;
}

/*-----------------------------------------------------------------------------
    Name        : etgEffectProfileKey
    Description : Script parsing callback for profiling an effect
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void etgEffectProfileKey(char *directory,char *field,void *dataToFillIn)
{
    sdword nScanned;
    char *string;

    etgEffectTestKey(directory, field, dataToFillIn);

    string = strtok(NULL, ETG_TokenDelimiters);
#if ETG_ERROR_CHECKING
    if (string == NULL)
    {
        dbgFatalf(DBG_Loc, "Error in %s\\%s: ProfileEffect format is '<keyChar>, <LightName>, <effectName>, <nTimes>, <duration>."ETG_Directory, ETG_Script);
    }
#endif
    nScanned = sscanf(string, "%d", &etgTestKey[etgTestKeyIndex - 1].nTimes);
#if ETG_ERROR_CHECKING
    if (nScanned != 1)
    {
        dbgFatalf(DBG_Loc, "Error in %s\\%s: ProfileEffect format is '<keyChar>, <LightName>, <effectName>, <nTimes>, <duration>."ETG_Directory, ETG_Script);
    }
#endif
    string = strtok(NULL, ETG_TokenDelimiters);
#if ETG_ERROR_CHECKING
    if (string == NULL)
    {
        dbgFatalf(DBG_Loc, "Error in %s\\%s: ProfileEffect format is '<keyChar>, <LightName>, <effectName>, <nTimes>, <duration>."ETG_Directory, ETG_Script);
    }
#endif
    nScanned = sscanf(string, "%f", &etgTestKey[etgTestKeyIndex - 1].duration);
#if ETG_ERROR_CHECKING
    if (nScanned != 1)
    {
        dbgFatalf(DBG_Loc, "Error in %s\\%s: ProfileEffect format is '<keyChar>, <LightName>, <effectName>, <nTimes>, <duration>."ETG_Directory, ETG_Script);
    }
#endif
#if ETG_VERBOSE_LEVEL >= 1
    dbgMessagef(" %d times in %.2f seconds.", etgTestKey[etgTestKeyIndex - 1].nTimes, etgTestKey[etgTestKeyIndex - 1].duration);
#endif
    regFunctionSet(etgTestKey[etgTestKeyIndex - 1].region, (regionfunction) etgEffectProfile);
}
#endif //ETG_TESTING

/*-----------------------------------------------------------------------------
    Name        : etgStartup
    Description : Starts the ETG module and loads in any files which need loading.
    Inputs      : void
    Outputs     : Load in some ETG files.
    Return      :
----------------------------------------------------------------------------*/
void etgStartup(void)
{
    sdword index = 0;
    sdword bigDeathInitialiser = -10;
    
#if ETG_VERBOSE_LEVEL >= 1
    dbgMessagef("Starting effects");
#endif
    //load in and set the default explosion and hit/fire
    memClearDword(etgDeathEventTable, 0, NUM_RACES * NUM_CLASSES * EDT_NumberExplosionTypes);
    memClearDword(etgDeathEventTableDerelict, 0, NUM_DERELICTTYPES * EDT_NumberExplosionTypes);
    memClearDword(etgGunEventTable, 0, NUM_RACES * NUM_GUN_SOUND_TYPES * EGT_NumberGunTypes);
    memClearDword(etgResourceEffectTable, 0, NUM_RACES * ETG_NumberResourceTypes);
    memClearDword(etgTractorBeamEffectTable, 0, NUM_RACES);
    memClearDword(etgSpecialPurposeEffectTable,0,EGT_NumberOfSpecialEffects);

    //default the big death amount to something
    memClearDword(etgBigDeathFactor,         (udword)bigDeathInitialiser, NUM_RACES * NUM_CLASSES);
    memClearDword(etgBigDeathFactorDerelict, (udword)bigDeathInitialiser, NUM_DERELICTTYPES);
    //set the default bullet colors for R1 and everything else
    memClearDword(etgBulletColor, R2BulletColor, NUM_RACES * NUM_GUN_SOUND_TYPES);
    memClearDword(etgBulletColor, R1BulletColor, NUM_GUN_SOUND_TYPES);
    //load in the rest of the effects
#if ETG_TESTING
    etgTestKeyIndex = 0;
#endif
    memset(etgMeshRegistry, 0, sizeof(etgMeshRegistry));    //initial clear out of the mesh registry
    memset(etgEventTable, 0, sizeof(etgEventTable));        //initial clear out of the event registry
    for (index = 0; etgScriptFilename[index] != NULL; index++)
    {
        scriptSet(ETG_Directory, etgScriptFilename[index], etgScriptParseTable);
    }
    etgNumberEffectsExpected = -1;
//    scriptSet(ETG_Directory, ETG_Script, etgScriptParseTable);
}

/*-----------------------------------------------------------------------------
    Name        : etgShutdown
    Description : Shuts down the effect module, freeing up all the old effects.
    Inputs      : void
    Outputs     : Frees all the effects loaded in etgStartup
    Return      : void
----------------------------------------------------------------------------*/
void etgShutdown(void)
{
    sdword index, j, k;

    //free all effect statics
#if ETG_VERBOSE_LEVEL >= 1
    dbgMessagef("Deleting up to %d effects", ETG_EventListLength);
#endif
    for (index = 0; index < ETG_EventListLength; index++)
    {
        if (etgEventTable[index].effectStatic != NULL)
        {
            etgEffectCodeDelete(etgEventTable[index].effectStatic, TRUE);
            etgEventTable[index].effectStatic = NULL;
        }
    }
    //free all meshes allocated for effects
    /*
    for (index = 0; index < etgMeshRegistryIndex; index++)
    {
        meshFree(etgMeshRegistry[index]);
    }
    */
//    etgMeshRegistryIndex = 0;
    etgMeshRegistryReset();
    //free all the effect LOD's
    for (index = 0; index < NUM_RACES; index++)
    {
        for (j = 0; j < NUM_CLASSES; j++)
        {
            for (k = 0; k < EDT_NumberExplosionTypes; k++)
            {
                if (etgDeathEventTable[index][j][k] != NULL)
                {
                    memFree(etgDeathEventTable[index][j][k]);
                }
            }
        }
    }
    for (index = 0; index < NUM_DERELICTTYPES; index++)
    {
        for (k = 0; k < EDT_NumberExplosionTypes; k++)
        {
            if (etgDeathEventTableDerelict[index][k] != NULL)
            {
                memFree(etgDeathEventTableDerelict[index][k]);
            }
        }
    }
    for (index = 0; index < NUM_RACES; index++)
    {
        for (j = 0; j < NUM_GUN_SOUND_TYPES; j++)
        {
            for (k = 0; k < EGT_NumberGunTypes; k++)
            {
                if (etgGunEventTable[index][j][k] != NULL)
                {
                    memFree(etgGunEventTable[index][j][k]);
                }
            }
        }
    }
    for (index = 0; index < NUM_RACES; index++)
    {
        for (j = 0; j < ETG_NumberResourceTypes; j++)
        {
            if (etgResourceEffectTable[index][j] != NULL)
            {
                memFree(etgResourceEffectTable[index][j]);
            }
        }
    }
    for (index = 0; index < NUM_RACES; index++)
    {
        if (etgTractorBeamEffectTable[index] != NULL)
        {
            memFree(etgTractorBeamEffectTable[index]);
        }
    }
    for (index = 0; index < EGT_NumberOfSpecialEffects; index++)
    {
        if (etgSpecialPurposeEffectTable[index] != NULL)
        {
            memFree(etgSpecialPurposeEffectTable[index]);
        }
    }
    for (index = 0; index < NUM_CLASSES; index++)
    {
        for (j = 0; j < DMG_NumberDamageTypes; j++)
        {
            if (etgDamageEffectTable[index][j] != NULL)
            {
                memFree(etgDamageEffectTable[index][j]);
            }
        }
    }
    if (etgHyperspaceEffect != NULL)
    {
        memFree(etgHyperspaceEffect);
    }
#if ETG_TESTING
    //free all the test keys
    for (index = 0; index < etgTestKeyIndex; index++)
    {
        memFree(etgTestKey[index].type);
        memFree(etgTestKey[index].effectName);
        memFree(etgTestKey[index].lightName);
        regRegionDelete(etgTestKey[index].region);
    }
    etgTestKeyIndex = 0;
#endif
}

/*-----------------------------------------------------------------------------
    Name        : etgReloadReset
    Description : Reset the already loaded ETG effects after a reload.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void etgReloadReset(void)
{
    sdword index, j;
    etgeffectstatic *newStatic;

    //recolorize all the meshes
    for (index = 0; index < ETG_NumberMeshes; index++)
    {                                                       //scan through the registry
        if (etgMeshRegistry[index].filename != NULL)
        {                                                   //if this guy has somethig allocated
            meshRecolorize(etgMeshRegistry[index].mesh);
        }
    }
    for (index = 0; index < ETG_EventListLength; index++)
    {
        if (etgEventTable[index].effectStatic != NULL)
        {                                                   //find a free event as we search
            newStatic = etgEventTable[index].effectStatic;
            for (j = 0; j < newStatic->nHistoryList; j++)
            {                                               //zero out the history
                newStatic->historyList[j] = REALlyNegative;
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : etgFixupUV
    Description : Adjust UV coordinates on ETG meshes to eliminate filtered oversampling
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void etgFixupUV(void)
{
    sdword index;

    for (index = 0; index < ETG_NumberMeshes; index++)
    {
        if (etgMeshRegistry[index].filename != NULL)
        {
            meshFixupUV(etgMeshRegistry[index].mesh);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : etgReset
    Description : Resets the etg module by freeing all the current effects and
                    loading them into the same address.
    Inputs      : void
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void etgReset(void)
{
    sdword index, j;
    char path[PATH_MAX];
    etgeffectstatic *newStat;
    char *nameSave;

#if ETG_VERBOSE_LEVEL >= 1
    dbgMessagef("Resetting up to %d effects", ETG_EventListLength);
#endif
    //free all meshes allocated for effects, they will be re-loaded later
    /*
    for (index = 0; index < etgMeshRegistryIndex; index++)
    {
        meshFree(etgMeshRegistry[index]);
    }
    etgMeshRegistryIndex = 0;
    */
    etgMeshRegistryReset();

    etgEventLoadCount++;                                    //update load count variable

    etgErrorRecoverable = TRUE;

    //reload all event statics
    for (index = 0; index < ETG_EventListLength; index++)
    {
        if (etgEventTable[index].effectStatic == NULL)
        {                                                   //if entry unused
            continue;
        }
        if (etgEventTable[index].loadCount < etgEventLoadCount)
        {                                                   //if this effect was loaded previously
            strcpy(path, ETG_Directory);                    //build a full path name
            strcat(path, etgEventTable[index].effectStatic->name);
            /*
            etgEffectCodeDelete(etgEventTable[index].effectStatic, FALSE);
            newStat = etgEffectCodeLoad(path);
            dbgAssertOrIgnore(newStat == etgEventTable[index].effectStatic);
            */
            nameSave = etgEventTable[index].effectStatic->name;//make sure the effect will be re-loaded
            etgEventTable[index].effectStatic->name = "DON'T LOAD ME";
            newStat = etgEffectCodeLoad(path);              //load in the effect
            etgEventTable[index].effectStatic->name = nameSave;//restore it's proper name
            if (newStat != NULL)                            //if the effect was loaded OK
            {
                etgEffectCodeDelete(etgEventTable[index].effectStatic, FALSE);
                memFree(etgEventTable[index].effectStatic->name);//free the name not freed by previous line
                *etgEventTable[index].effectStatic = *newStat;//delete the registry entry
                for (j = 0; j < ETG_EventListLength; j++)
                {                                           //find effect just loaded
                    if (etgEventTable[j].effectStatic == newStat)
                    {                                       //make sure the effect static pointer is in the same loacation as originally loaded
                        etgEventTable[j].effectStatic = NULL;
                        break;
                    }
                }
            }                                               //else retain the previously loaded effect
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : etgEffectCodeExecute
    Description : Execute a block of code.
    Inputs      : stat - static info for the effect
                  effect - local effect who has all the goods
                  codeBlock - which code block to execute
    Outputs     : Executes the specified p-code
    Return      : void
----------------------------------------------------------------------------*/
//#define ETG_ExecStackDepth   4
//global variables for execution of a chunk of code
typedef struct
{
    etgcodeblock etgCodeBlock[ETG_NumberCodeBlocks];
    //ubyte *etgCurrentCodeBlock;
    udword etgCodeBlockIndex;
    //udword etgCodeOffset;
    //udword etgCodeLength;
    ubyte *etgVariables;
    bool32 etgDeleteFlag;
}
etgeffectstack;
etgeffectstack etgExecStack;//[ETG_ExecStackDepth];
//sdword etgExecStackIndex = -1;
void etgEffectCodeExecute(etgeffectstatic *stat, Effect *effect, udword codeBlock)
{
    udword opcode;
    sdword size;
    ubyte *pOpcode;

	//this function does not interface well with optimized code which assumes 
	//certain variables will not get stomped, hence the pushes
    etgExecStack.etgCodeBlockIndex = codeBlock;
    etgExecStack.etgCodeBlock[EPM_Startup].offset = etgExecStack.etgCodeBlock[EPM_EachFrame].offset = etgExecStack.etgCodeBlock[EPM_TimeIndex].offset = 0;
    etgExecStack.etgVariables = effect->variable;

    while (etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].offset <
           etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].length)
    {
		
        pOpcode = etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].code + etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].offset;
        opcode = *((udword *)pOpcode);                      //get an opcode
#if ETG_ERROR_CHECKING
        if (opcode < 0 || opcode >= EOP_LastOp)
        {
            dbgFatalf(DBG_Loc, "Effect '%s' has a bad opcode in code segment %d offset %d", stat->name, etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].code, etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].offset);
        }
        if (etgHandleTable[opcode].function == NULL)
        {
            dbgFatalf(DBG_Loc, "etgEffectCodeExecute: NULL opcode %d", opcode);
        }
#endif
        //execute the opcode
        size = etgHandleTable[opcode].function(effect, stat, pOpcode);
        etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].offset += size;
		
    }
}

/*-----------------------------------------------------------------------------
    Name        : etgCodeMakeCurrent
    Description : Make a particular efect static's code blocks current
    Inputs      : stat - code static to make current
    Outputs     : Copies the etgcodeblock structures from the effect static to
                    etgCodeBlocks.
    Return      : void
----------------------------------------------------------------------------*/
void etgCodeMakeCurrent(etgeffectstatic *stat)
{
    memcpy(etgExecStack.etgCodeBlock, stat->codeBlock, sizeof(etgcodeblock) * ETG_NumberCodeBlocks);
}

/*-----------------------------------------------------------------------------
    Name        : etgEffectCodeStart
    Description : Starts up the code, variables etc. of an effect in a useable form.
    Inputs      : effect - effect we're starting up
                  nParams - number of user parameters
                  params - CAT: the user parameters as an array (etgEffectCodeStart 0x0046a5b0 reads them through its
                    4th argument; HW1 took them as variable arguments)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void etgEffectCodeStart(etgeffectstatic *stat, Effect *effect, sdword nParams, etgarg *params)
{
    sdword index;


#if ETG_ERROR_CHECKING
    if (stat->nParticleBlocks == 0xffffffff)
    {
        dbgFatalf(DBG_Loc, "Effect '%s' not loaded properly", stat->name);
    }
#endif
    //set the variable and variable rate blocks
    if (stat->variableSize)
    {
//        effect->variable = memAlloc(stat->variableSize + stat->rateSize +
//                                    stat->variableSize / 4, "EffectVariables", 0);
        effect->variable = ((ubyte *)effect) + etgEffectSize(stat->nParticleBlocks);
        effect->variableRate = effect->variable + stat->variableSize;
        effect->effectID = effect->variableRate + stat->rateSize;
        memset(effect->effectID, 0xff, stat->variableSize / SCALESHIFT(4)); //clear out the effect ID's
        memset(effect->variableRate, 0, stat->rateSize);
    }
    else
    {
        effect->variable = effect->variableRate = effect->effectID = NULL;
    }
    //set the particle block info
    effect->nParticleBlocks = stat->nParticleBlocks;
    effect->iParticleBlock = 0;
    memset(effect->particleBlock, 0, effect->nParticleBlocks * sizeof(ubyte *));
    //init the variables to initial values
    for (index = 0; index < stat->initLength; index++)
    {
        ((memsize *)effect->variable)[stat->variableOffsets[index]] =
            stat->variableInitData[index];
    }
    //init the user parameters, if any
    // CAT-FIX (M2-R8 oracle-rung-physics): the exe writes nParams parameters with no bound (etgEffectCodeStart
    //      0x0046a684); the tree stops at the end of the effect's variable block, where the exe would write past it
    for (index = 0; index < nParams; index++)
    {
        if ((udword)(index + 1) * sizeof(memsize) > (udword)stat->variableSize)
        {
            break;
        }
        ((memsize *)effect->variable)[index] = params[index];
    }
    //clear out the flags list
    effect->effectFlags = 0;
    //no spin yet
    effect->spin = effect->deltaSpin = 0.0f;
    //update the effect's history, if applicable
    etgHistoryRegister(stat);
}

/*-----------------------------------------------------------------------------
    Name        : etgEffectDelete
    Description : Deletes an effect just before it is removed from mission spheres etc.
    Inputs      :
    Outputs     : Does not memFree the effect
    Return      :
----------------------------------------------------------------------------*/
void etgEffectDelete(Effect *effect)
{
    sdword index;

    // CAT: the effect's one-byte block goes first (etgEffectDelete 0x0046a6f0, and inlined in etgDeleteEffectsOwnedBy
    //      0x0046fff0 at 0x0047001d)
    cat_univFreeEffectBlock(&effect->obj);
    // CAT: ships drop their references to the effect (cat_univRemoveEffectReferencesFromShips 0x005475e0, called by
    //      etgEffectDelete 0x0046a6f0 and etgDeleteEffectsOwnedBy 0x0046fff0)
    cat_univRemoveEffectReferencesFromShips(effect);
    for (index = 0; index < effect->iParticleBlock; index++)
    {
        if (effect->particleBlock[index] != NULL)
        {
            // CAT: lens flare sources go first (cat_partSystemFreeAttachments 0x004e2500), and the block pointer is
            //      cleared after the free (0x0046a746)
            cat_partSystemFreeAttachments((psysPtr)effect->particleBlock[index]);
            memFree(effect->particleBlock[index]);
            effect->particleBlock[index] = NULL;
        }
    }
    if (bitTest(effect->staticinfo->specialOps, ESO_WorldRender))
    {
        univRemoveFromWorldList((Derelict *)effect);
    }
    listRemoveNode(&effect->effectLink);
}

/*-----------------------------------------------------------------------------
    Name        : etgEffectCodeDelete
    Description : Deletes an effect static
    Inputs      : stat - effect static to delete
                  bFullDelete - if TRUE will memFree the actual static structure itself.
    Outputs     : Frees all memory allocated to the effect static, including
                    the effect static structure itself.
    Return      : void
----------------------------------------------------------------------------*/
void etgEffectCodeDelete(etgeffectstatic *stat, bool32 bFullDelete)
{
    if (bFullDelete)
    {
        memFree(stat->name);
    }
    // CAT: the test is in the exe's release build too (etgEffectCodeDelete 0x0046a79e), not only under
    //      ETG_ERROR_CHECKING: a placeholder static (named by a spawn, never loaded) has nothing else to free
    if (stat->nParticleBlocks == 0xffffffff)
    {
        dbgWarningf(DBG_Loc, "etgEffectCodeDelete: Effect '%s' not loaded properly", stat->name);
        if (bFullDelete)
        {
            memFree(stat);
        }
        return;
    }
//    memFree(stat->variableOffsets);
    if (stat->variableInitData)
    {
        memFree(stat->variableInitData);
        stat->variableInitData = NULL;
    }
    if (stat->constData)
    {
        memFree(stat->constData);
        stat->constData = NULL;
    }
    if (stat->decisions)
    {
        memFree(stat->decisions);
        stat->decisions = NULL;
    }
    if (stat->alternateOffsets)
    {
        memFree(stat->alternateOffsets);
        stat->alternateOffsets = NULL;
    }
    if (stat->codeBlock[EPM_Startup].code != NULL)
    {
        memFree(stat->codeBlock[EPM_Startup].code);
        stat->codeBlock[EPM_Startup].code = 0;
    }
    if (stat->codeBlock[EPM_EachFrame].code != NULL)
    {
        memFree(stat->codeBlock[EPM_EachFrame].code);
        stat->codeBlock[EPM_EachFrame].code = 0;
    }
    if (stat->codeBlock[EPM_TimeIndex].code != NULL)
    {
        memFree(stat->codeBlock[EPM_TimeIndex].code);
        stat->codeBlock[EPM_TimeIndex].code = 0;
    }
    if (stat->nHistoryList > 0)
    {
        memFree(stat->historyList);
        stat->historyList = NULL;                           // CAT: cleared (0x0046a861)
    }
    stat->nParticleBlocks = 0xffffffff;
    if (bFullDelete)
    {
        memFree(stat);
    }
}

/*-----------------------------------------------------------------------------
    Name        : etgEffectUpdate
    Description : Update a specified effect
    Inputs      : effect - effect we are to update
                  timeElapsed - time elapsed since effect last updated
    Outputs     : executes code segments which will do whatever they will.
    Return      : TRUE if we are to delete the effect, false otherwise
----------------------------------------------------------------------------*/
real32 etgTimeElapsed;
real32 etgTotalTimeElapsed;
bool32 etgEffectUpdate(Effect *effect, real32 timeElapsed)
{
    etgeffectstatic *stat = effect->staticinfo;
    matrix tempMatrix, rotMatrix;
    real32 spin;

    etgExecStack.etgDeleteFlag = FALSE;                     //set some global parameters
    etgTimeElapsed = timeElapsed;
    etgTotalTimeElapsed = effect->timeElapsed + timeElapsed;
    effect->timeElapsed += timeElapsed;                     //record additional time elapsed
    if (etgTotalTimeElapsed < 0.0f)
    {                                                       //if effect still waiting to start
        return(FALSE);
    }
    etgCodeMakeCurrent(stat);
    if (etgTotalTimeElapsed <= timeElapsed + 0.005)
    {                                                       //if effect just started
        etgEffectCodeExecute(stat, effect, EPM_Startup);
    }

    //execute the time index block
    etgEffectCodeExecute(stat, effect, EPM_TimeIndex);
    if (etgExecStack.etgDeleteFlag != TRUE)
    {   //execute the eachFrame
        etgEffectCodeExecute(stat, effect, EPM_EachFrame);
    }
    //update the physics of the effect
    if (effect->owner)
    {
        if (bitTest(effect->header.flags, SOF_AttachCoordsys))
        {
            effect->rotatable.rotinfo.coordsys = effect->owner->rotatable.rotinfo.coordsys;
        }
        if (bitTest(effect->header.flags, SOF_AttachPosition))
        {
            effect->obj.posinfo.position = effect->owner->obj.posinfo.position;
        }
        if (bitTest(effect->header.flags, SOF_AttachVelocity))
        {
            effect->obj.posinfo.velocity = effect->owner->obj.posinfo.velocity;
        }
        else
        {
            vecMultiplyByScalar(effect->obj.posinfo.velocity, effect->drag);
        }
        if (bitTest(effect->header.flags, SOF_AttachNLips))
        {
            effect->magnitudeSquared = effect->owner->magnitudeSquared;
        }
    }
    else
    {
        vecMultiplyByScalar(effect->obj.posinfo.velocity, effect->drag);
    }
    vecAddToScalarMultiply(effect->obj.posinfo.position, effect->obj.posinfo.velocity, timeElapsed);
    effect->spin += effect->deltaSpin;
    if (effect->spin != 0.0f)
    {
        //build a rotation matrix
        spin = effect->spin * etgTimeElapsed;
        matMakeRotAboutZ(&rotMatrix, (real32)sin((double)spin), (real32)cos((double)spin));
        tempMatrix = effect->rotatable.rotinfo.coordsys;
        //rotate the effect
        matMultiplyMatByMat(&effect->rotatable.rotinfo.coordsys, &rotMatrix, &tempMatrix);
        //rotate all particle systems
    }
    //return the proper flag to ensure the effect was rendered at least once
    if (bitTest(effect->effectFlags, EF_Rendered))
    {
        return(etgExecStack.etgDeleteFlag | (effect->effectFlags & EM_ToBeDeleted));
    }
    else if (etgExecStack.etgDeleteFlag | (effect->effectFlags & EM_ToBeDeleted))
    {
        effect->effectFlags += EF_ToBeDeleted;
        if ((effect->effectFlags & EM_ToBeDeleted) > EM_ForceDelete)
        {
            return(TRUE);
        }
    }
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : etgEffectDraw
    Description : Draw an effect
    Inputs      : effect - effect to draw
    Outputs     :
    Return      :
    Note        : it is assumed that the effect's matrix has already been applied
----------------------------------------------------------------------------*/
void etgEffectDraw(Effect *effect)
{
    sdword index;
    hmatrix coordMatrixX;
    matrix scaledEffectSys;
    pointSystem *part;
    real32 timeElapsed;
    vector velInverse;

    //!!!ASSERT: glMatrixMode == GL_MODELVIEW

    if (effect->timeElapsed < 0.0f)
    {                                                       //if effect hasn't started yet
        return;
    }
    //make sure the effect's N-Lips is up-to-date
    if (effect->owner)
    {
        if (bitTest(effect->header.flags, SOF_AttachNLips))
        {
            effect->magnitudeSquared = effect->owner->magnitudeSquared;
        }
    }
    //compute a N-Lipped effect coord sys
    partNLips = effect->magnitudeSquared;
    matCopyAndScale(&scaledEffectSys, &effect->rotatable.rotinfo.coordsys, partNLips);
    //compute inverse of that coord sys
    //matMultiplyVecByMat(&velInverse, &effect->obj.posinfo.velocity, &effect->rotatable.rotinfo.coordsys);
    matMultiplyVecByMat(&velInverse, &effect->obj.posinfo.velocity, &scaledEffectSys);
    hmatMakeHMatFromMat(&coordMatrixX,&scaledEffectSys);
    hmatPutVectIntoHMatrixCol4(effect->obj.posinfo.position,coordMatrixX);
    for (index = 0; index < effect->iParticleBlock; index++)
    {
        if (effect->particleBlock[index] != NULL)
        {
            part = (pointSystem *)effect->particleBlock[index];
            timeElapsed = universe.totaltimeelapsed - part->lastUpdated;
            if (timeElapsed >= 0)
            {
				partUpdateSystem((psysPtr)effect->particleBlock[index], timeElapsed, &velInverse); //update the particle
                part->lastUpdated = universe.totaltimeelapsed;
            }

            if (bitTest(part->flags, PART_WORLDSPACE))
            {
                if ((effect->owner != NULL) &&
                    (effect->owner->header.objtype == OBJ_ShipType))
                {
                    //this is for the benefit of the hyperspace effect
                    partEffectOwnerSystem   = scaledEffectSys;
                    partEffectOwnerPosition = effect->obj.posinfo.position;
                    partEffectColor         = effect->owner->staticinfo->hyperspaceColor;
                }
                partRenderSystem((psysPtr)effect->particleBlock[index]);
            }
            else if (bitTest(effect->header.flags, SOF_OwnerCoordSys))  // CAT: etgEffectDraw tests 0x20000000
            {
                hmatrix ownerCoordMatrix;
                matrix tempMatrix;

                dbgAssertOrIgnore(effect->owner != NULL);

                matCopyAndScale(&tempMatrix, &effect->owner->rotatable.rotinfo.coordsys, effect->owner->magnitudeSquared);
                //hmatMakeHMatFromMat(&ownerCoordMatrix, &effect->owner->rotatable.rotinfo.coordsys);
                hmatMakeHMatFromMat(&ownerCoordMatrix, &tempMatrix);
                hmatPutVectIntoHMatrixCol4(effect->owner->obj.posinfo.position, ownerCoordMatrix);

                glPushMatrix();
                glMultMatrixf((GLfloat*)&ownerCoordMatrix);
                glMultMatrixf((GLfloat*)&coordMatrixX);

                partRenderSystem((psysPtr)effect->particleBlock[index]);  //render the particle

                glPopMatrix();
            }
            else
            {
                if (partTypeof(effect->particleBlock[index]) == PART_BILLBOARD)
                {
                    partModifyBillPosition((psysPtr)effect->particleBlock[index],
                                       &effect->obj.posinfo.position);
                }
                glPushMatrix();
                glMultMatrixf((float *)&coordMatrixX);      //effect's rotation matrix
                partRenderSystem((psysPtr)effect->particleBlock[index]);
                glPopMatrix();
            }
        }
    }
    bitSet(effect->effectFlags, EF_Rendered);
/*
    if (worldRendered)
    {
        glEnable(GL_DEPTH_TEST);
        rndNormalizeEnable(FALSE);
//        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf((GLfloat*)&rndProjectionMatrix);
        glMatrixMode(GL_MODELVIEW);
    }
*/
}

/*-----------------------------------------------------------------------------
    Effect-loading functions:
-----------------------------------------------------------------------------*/
filehandle etgFileHandle = 0;                //handle of file we're reading
char *etgFileName = NULL;
sdword etgFileLine;                             //current line in that file
sdword etgParseMode;                            //current parsing mode
#define STRING_LENGTH           256
#define OPCODE_MAX              524
#define ETG                     etgFileName, etgFileLine - 1
#define ETG_U                   etgFileName, -1
//code blocks where the code is composed to
/*
ubyte *etgCodeStartup;
ubyte *etgCodeEachFrame;
ubyte *etgCodeTimeIndex;
sdword etgOffsetStartup;
sdword etgOffsetEachFrame;
sdword etgOffsetTimeIndex;
*/
//variable block used during parsing
etgvarentry *etgVariableTable;
sdword etgVariableIndex;
//brace-nesting stack
etgnestentry etgNestStack[ETG_NestStackDepth];
sdword etgNestLevel;
//alternate/case-making stuff
sdword etgTotalOffsets;
etgdecision *etgDecisionOpcode = NULL;
//udword etgAlternateIndex;
etgextalt *etgAlternateTable;
sdword etgAlternateStructIndex;
//stuff for time index creation
sdword etgTimeIndexIndex = 0;
//constant data pool
ubyte *etgConstData;

/*-----------------------------------------------------------------------------
    Name        : etgLoadError
    Description : Handle an ETG parse error with error recovery if applicable.
    Inputs      : same as dbgFatal
    Outputs     :
    Return      : 0
----------------------------------------------------------------------------*/
sdword etgLoadError(char *file, sdword line, char *error)
{
    if (etgErrorRecoverable)
    {
        dbgNonFatal(file, line, error);
    }
    else
    {
        dbgFatal(file, line, error);
    }
    return(0);
}

/*-----------------------------------------------------------------------------
    Name        : etgLoadErrorf
    Description : Handle an ETG parse error with error recovery and variable parameters.
    Inputs      : same as dbgFatalf
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
sdword etgLoadErrorf(char *file, sdword line, char *format, ...)
{
    va_list argList;
    char error[DBG_BufferLength];

    va_start(argList, format);                              //get first arg
    vsprintf(error, format, argList);                       //prepare output string
    va_end(argList);

    // CAT: always fatal (etgLoadErrorf 0x0046ae90 calls dbgFatal directly; HW1's recoverable path through
    //      etgLoadError, used by the effects reload key, is gone from it)
    dbgFatal(file, line, error);                            //print the message
    return(0);
}

/*-----------------------------------------------------------------------------
    Name        : etgLineRead
    Description : Read a single line from the etg file and put it into the correct format
    Inputs      : dest - where to store the destination string
                  length - length of string
    Outputs     : reads and beautifies a string
    Return      : NULL if end of file, dest otherwise
----------------------------------------------------------------------------*/
char *etgLineRead(char *dest, sdword length)
{
    char string[STRING_LENGTH];
    char *pString, *pStart, *pEnd, *pDest;
    ERR_TYPE exprError;
    double exprValue;

    //read in lines until we get a non-blank one
    do
    {
        //read in a line
        if (fileLineRead(etgFileHandle, string, length) == FR_EndOfFile)
        {
            return(NULL);
        }
        etgFileLine++;
        //strip line comments, if any.  There shouldn't be any
        if ((pString = strstr(string, "//")) != NULL)
        {
            *pString = 0;
        }
        //strip leading spaces
//        pStart = pEnd = NULL;
        for (pStart = pString = string; ; pString++)
        {                                                   //find first non-whitespace char
            if (strchr(" \t", *pString) == NULL)
            {
                pStart = pString;
                break;
            }
            if (*pString == 0)
            {
                pStart = pString;
                break;
            }
        }
        //strip trailing spaces
        for (pEnd = pString = string + strlen(string); pString > string; pString--)
        {                                                   //find last non-whitespace character
            if (strchr(" \t\n", *pString) == NULL)
            {                                               //if non-whitespace
                pEnd = pString + 1;                         //end of string
                break;
            }
        }
        *pEnd = 0;                                          //remove the trailing spaces
    }
    while (strlen(pStart) < 1);
    //search for, and expand all expressions
    pDest = dest;
    dest[0] = 0;
    while ((pString = strchr(pStart, '[')) != NULL)
    {                                                       //while there is an expression visible
        //exploit the strncopy NULL-terminator strangeness
        strncpy(pDest, pStart, pString - pStart);           //copy up to the bracket
        pDest += pString - pStart;
        *pDest = 0;
#if ETG_ERROR_CHECKING
        if (strchr(pString + 1, ']') == NULL)
        {                                                   //if no closing bracket
            etgLoadErrorf(ETG, "'[' without ']'");
            return(NULL);
        }
#endif
        pEnd = strchr(pString, ']');                        //get closing bracket
        *pEnd = 0;                                          //null-terminate this expression
#if ETG_ERROR_CHECKING
        exprError = evalSyntaxOK(pString + 1);              //check expression syntax
        if (exprError != ALL_OK)
        {
            etgLoadErrorf(ETG, "%s in expression '%s'", evalErrorString(exprError), pString + 1);
            return(NULL);
        }
#endif
        exprValue = evalEvaluate(pString + 1, &exprError);  //evaluate expression
#if ETG_ERROR_CHECKING
        if (exprError != ALL_OK)
        {
            etgLoadErrorf(ETG, "%s in expression '%s'", evalErrorString(exprError), pString + 1);
            return(NULL);
        }
#endif
        evalNum2Str(exprValue, pDest);                      //print the string out
        pDest += strlen(pDest);                             //update the dest pointer
        pStart = pEnd + 1;                                  //update the source pointer
    }
    strcpy(pDest, pStart);                                  //copy the whole expression
    return(dest);
}

/*-----------------------------------------------------------------------------
    Name        : etgParseNamesMatch
    Description : A specialized sort of string compare, this function compares
                    two strings.  The first n characters of string must be equal
                    to nameToMatch.
    Inputs      : string - string to seach in
                  nameToMatch - name to, well, match
    Outputs     : ..
    Return      : length of nameToMatch of they match, or 0 if they don't
----------------------------------------------------------------------------*/
char etgLabelString[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz12345667890-_{}#";
sdword etgParseNamesMatch(char *string, char *nameToMatch)
{
    sdword countS, countN;

    for (countS = 0; string[countS] && strchr(etgLabelString, string[countS]); countS++);
    for (countN = 0; nameToMatch[countN] && strchr(etgLabelString, nameToMatch[countN]); countN++);
    if (countN != countS || strncmp(string, nameToMatch, countN) != 0)
    {
        return(0);
    }
    return(countN);
}

/*-----------------------------------------------------------------------------
    Name        : etgParametersIsolate
    Description : Isolate the parameter block (within parenteses) from a string.
    Inputs      : string - string to look in
                  offset starting offset in that string
    Outputs     : kills the parentheses (converts them to zero)
    Return      : pointer to start of parameter block
----------------------------------------------------------------------------*/
char *etgParametersIsolate(char *string, sdword offset)
{
#if ETG_ERROR_CHECKING
    char *temp;
    if ((temp = strchr(string, '(')) == NULL)
    {
        etgLoadErrorf(ETG, "Expecting parameters in '%s'", string);
        return(NULL);
    }
    if (!strchr(temp, ')'))
    {
        etgLoadErrorf(ETG, "No closing parenthesis in '%s'", string);
        return(NULL);
    }
#endif
    string = strchr(string, '(');
    *string = 0;                                            //kill opening perenthesis
    string++;
    *strchr(string, ')') = 0;                               //kill closing parenthesis
    return(string);
}

/*-----------------------------------------------------------------------------
    Name        : etgEffectStaticFind
    Description : Find a named effect
    Inputs      : name - name of effect to find
                  bRegister - if TRUE, the effect will be registered for later
                    loading.  Usually set to FALSE
    Outputs     : If effect not already registered, a new effect will be created
                    with a given name to be loaded in later.
    Return      : pointer to effect, or NULL if not already registered
----------------------------------------------------------------------------*/
etgeffectstatic *etgEffectStaticFind(char *name, bool32 bRegister)
{
    sdword index;
    etgeffectstatic *newStatic;
    char *slash;
    etgevent *free = NULL;

    //strip all the path crap off, so we have just a name
#ifdef _WIN32
    for (slash = strchr(name, '\\'); slash != NULL; slash = strchr(slash + 1, '\\'))
#else
    for (slash = strpbrk(name, "\\/"); slash != NULL; slash = strpbrk(slash + 1, "\\/"))
#endif
    {
        name = slash + 1;
    }
    //see if it'a already registered
    for (index = 0; index < ETG_EventListLength; index++)
    {
        if (etgEventTable[index].effectStatic == NULL)
        {                                                   //find a free event as we search
            if (free == NULL)
            {
                free = &etgEventTable[index];
            }
        }
        else
        {
            if (strcasecmp(etgEventTable[index].effectStatic->name, name) == 0)    // CAT: _stricmp (etgEffectStaticFind 0x0046b090)
            {                                                   //if names match
                return(etgEventTable[index].effectStatic);
            }
        }
    }

    if (bRegister == FALSE)
    {
        return(NULL);
    }
    //if it gets here, event doesn't exist.  If this is the case, let's allocate
    //a new blank effect static to be filled in later.
#if ETG_ERROR_CHECKING
    if (free == NULL)
    {
        dbgFatalf(DBG_Loc, "No room in event list to load '%s'", name);
    }
    /*
    if (etgEventIndex >= ETG_EventListLength)
    {
        etgLoadErrorf(ETG, "Exceeded %d effects.", ETG_EventListLength);
    }
    */
#endif
    newStatic = memAlloc(sizeof(etgeffectstatic), "EffectStatic", NonVolatile);
    newStatic->name = memStringDupe(name);                  // CAT: memStringDupe (etgEffectStaticFind 0x0046b090), HW1 memStringDupeNV
    newStatic->nParticleBlocks = 0xffffffff;                //mark as not yet allocated
    free->effectStatic = newStatic;                         //store the effect reference
    free->loadCount = etgEventLoadCount;                    //store the effect load counter
//    etgEventIndex++;
    return(newStatic);
}

/*-----------------------------------------------------------------------------
    Name        : etgOpcodeScan
    Description : Scan to see if we can create an opcode from the specified string.
    Inputs      : stat - effect static data we're working on
                  string - sring representing the code for the line
                  dest - where to store the opcode generated, if any
                  destLength - where to store the length of the opcode
    Outputs     : May generate and opcode and store it in dest, the length of
                    it in length.
    Return      : TRUE if a processor function found
----------------------------------------------------------------------------*/
bool32 etgOpcodeScan(struct etgeffectstatic *stat, char *string, ubyte *dest, sdword *destLength)
{
    sdword index;
    sdword strLength;
    char *start, *params;
    char returnValue[STRING_LENGTH];
    bool32 bReturnValue;

    //find return value name, if any
    if ((start = strstr(string, "<-")) != NULL)             //if found an assignment sign
    {
        memcpy(returnValue, string, start - string - 1);    //duplicate the return value bit
        returnValue[start - string - 1] = 0;                //NULL-terminate it
        strtok(returnValue, " \t");                         //remove trailing spaces
        start += strlen("<-");                              //skip assignment symbol
        while (strchr(" \t", *start) != NULL)
        {                                                   //find next non-whitespace
            start++;
        }
        bReturnValue = TRUE;
    }
    else
    {
        start = string;
        bReturnValue = FALSE;
    }
    //see if there is an opcode match anywhere
    for (index = 0; etgParseTable[index].name != NULL; index++)
    {                                                       //for all parse table entries
        strLength = etgParseNamesMatch(start, etgParseTable[index].name);
        if (strLength > 0)                                  //if found a match
        {
#if ETG_ERROR_CHECKING
            if (!bReturnValue)
            {                                               //if no return value
                if (bitTest(etgParseTable[index].flags, PTF_Return))
                {                                           //if we need a return value
                    etgLoadErrorf(ETG, "Expression '%s' requires a return parameter.", etgParseTable[index].name);
                    return(FALSE);
                }
            }
            else
            {
                if (!bitTest(etgParseTable[index].flags, PTF_ReturnOptional))
                {
                    etgLoadErrorf(ETG, "Expression '%s' has no return parameter.", etgParseTable[index].name);
                    return(FALSE);
                }
            }
#endif
            if (bitTest(etgParseTable[index].flags, PTF_Parameters))
            {                                               //isolate parameters appropriately
                params = etgParametersIsolate(start, strLength);
            }
            else if (bitTest(etgParseTable[index].flags, PTF_RestOfLine))
            {
                params = start + strLength;
                while (strchr(" \t", *params))
                {
                    params++;                               //find next non-whitespace
                }
            }
            else
            {
                params = NULL;
            }
            //call the parsing function
            *destLength = etgParseTable[index].function(stat, dest, start, params, returnValue);
            dbgAssertOrIgnore(0 <= *destLength && *destLength < OPCODE_MAX);
            return(TRUE);
        }
    }
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : etgIsNumber
    Description : Determine if a string is a number
    Inputs      : number - string to check numerality of
    Outputs     :
    Return      : TRUE if a number, false otherwise
----------------------------------------------------------------------------*/
bool32 etgIsNumber(char *number)
{
    while (*number)
    {
        if (strchr("0123456789.-", *number) == NULL)
        {
            return(FALSE);
        }
        number++;
    }
    return(TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : etgVariableFind
    Description : Find a variable, it it exists
    Inputs      : name - name of variable to find
    Outputs     :
    Return      : variable info structure if found or NULL if not found
----------------------------------------------------------------------------*/
etgvarentry *etgVariableFind(char *name)
{
    sdword index;

    for (index = 0; index < etgVariableIndex; index++)
    {
        if (strcasecmp(etgVariableTable[index].name, name) == 0)//if found a matching variable (CAT: case-insensitive, _stricmp in every exe lookup of the variable table)
        {
            return(&etgVariableTable[index]);               //return the variable found
        }
    }
    return(NULL);
}

/*-----------------------------------------------------------------------------
    Name        : etgColorScan
    Description : Attemp to scan a color from string
    Inputs      : string - string to scan.  There must be 3 or 4 comma-separated
                    values representing red, green, blue and alpha.  Alpha is
                    optional.  These numbers are treated as floating values in
                    the range of 0..1
    Outputs     :
    Return      : scanned color
    Note        : will generate an error if invalid string
----------------------------------------------------------------------------*/
color etgColorScan(char *string)
{
    sdword nScanned;
    real32 red, green, blue, alpha;

    nScanned = sscanf(string, "%f,%f,%f,%f", &red, &green, &blue, &alpha);
    if (nScanned == 3)
    {
        alpha = 1.0f;
    }
#if ETG_ERROR_CHECKING
    else if (nScanned != 4)
    {
        etgLoadErrorf(ETG, "Error scanning '%s' for a color.", string);
        return(0);
    }
#endif
    return(colRGBA(colRealToUbyte(red), colRealToUbyte(green),
                   colRealToUbyte(blue), colRealToUbyte(alpha)));
}

/*-----------------------------------------------------------------------------
    Name        : etgVarCopyScan
    Description : Scan for a variable assignment to a constant or other variable
    Inputs      : same as for etgOpcodeScan
    Outputs     : Creates an opcode if one found and stores the length in *destLength
    Return      : TRUE if a scanned properly, FALSE if found
----------------------------------------------------------------------------*/
bool32 etgVarCopyScan(struct etgeffectstatic *stat, char *string, ubyte *dest, sdword *destLength)
{
    char *rValue, *lValue, *parser;
    etgvariablecopy *opcode = (etgvariablecopy *)dest;
    etgvarentry *rVar, *lVar;
    udword constant, offset;
    sdword nScanned;

    //find assignment statement, if one exists
    if ((rValue = strstr(string, "<-")) != NULL)            //if found an assignment sign
    {
        lValue = strtok(string, " \t<-");                   //get left and right values
        rValue = strtok(NULL, " \t<-");

        //figure out the l-value (variable only)
        lVar = etgVariableFind(lValue);                     //find the variable
#if ETG_ERROR_CHECKING
        if (lVar == NULL)
        {
            etgLoadErrorf(ETG, "Variable '%s' not found.", lValue);
            return(FALSE);
        }
        if (lVar->type == EVT_Label || lVar->type == EVT_ConstLabel || lVar->type == EVT_VarLabel)
        {
            etgLoadErrorf(ETG, "Cannot set label '%s'.", lValue);
            return(FALSE);
        }
        if (lVar->type == EVT_Pointer)
        {
            etgLoadErrorf(ETG, "!!!Cannot do pointer types yet.");
            return(FALSE);
        }
#endif
        opcode->dest = lVar->offset;

        //figure out the r-value (constant or variable)
        if (etgIsNumber(rValue))
        {                                                   //assigning a constant
            if (lVar->type == EVT_Float)
            {                                               //determine how to scan
                parser = "%f";
            }
            else
            {
                parser = "%d";
            }
            nScanned = sscanf(rValue, parser, &constant);
#if ETG_ERROR_CHECKING
            if (nScanned != 1)
            {
                etgLoadErrorf(ETG, "Bad format for constant '%s'.", rValue);
                return(FALSE);
            }
#endif
            opcode->opcode = EOP_VariableAssign;
            opcode->source = constant;
            *destLength = sizeof(etgvariablecopy);
        }
        else
        {                                                   //else r-value is a variable
            //get variable
            rVar = etgVariableFind(rValue);
#if ETG_ERROR_CHECKING
            if (rVar == NULL)
            {
                etgLoadErrorf(ETG, "Variable '%s' not found.", rValue);
                return(FALSE);
            }
//          if (rVar->type == EVT_Label)
//          {
//              etgLoadErrorf(ETG, "Cannot pass label '%s' to function.", rValue);
//          }
            if (rVar->type == EVT_Pointer)
            {
                etgLoadErrorf(ETG, "!!!Cannot do pointer types yet.");
                return(FALSE);
            }
#endif
            switch (rVar->type)
            {                                               //adjust opcode based on var type
#if ETG_ERROR_CHECKING
                case EVT_Label:
                    etgLoadErrorf(ETG, "Cannot use label '%s' as an r-value", rValue);
                    return(FALSE);
                    break;
#endif
                case EVT_ConstLabel:
                case EVT_VarLabel:
                    opcode->opcode = EOP_VariableAssign;    //setting to label, same as
                    offset = rVar->initial;
                    break;                                  //assigning a constant
                default:
                    opcode->opcode = EOP_VariableCopy;
                    offset = rVar->offset;
                    break;
            }
            opcode->source = offset;
            *destLength = sizeof(etgvariablecopy);
        }
        return(TRUE);
    }
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : etgFunctionCallScan
    Description : Scan for a matching function call name
    Inputs      : same as for etgOpcodeScan
    Outputs     : Creates an opcode if one found and stores the length in *destLength
    Return      : TRUE if a function found, FALSE if not found
----------------------------------------------------------------------------*/
bool32 etgFunctionCallScan(struct etgeffectstatic *stat, char *string, ubyte *dest, sdword *destLength)
{
    sdword index, varIndex;
    sdword strLength, nScanned;
    udword constant, offset;
    char *start, *params, *param, *parser;
    char returnValue[STRING_LENGTH];
    bool32 bReturnValue;
    etgfunctioncall *opcode = (etgfunctioncall *)dest;
    etgvarentry *variable;

    //find return value name, if any
    if ((start = strstr(string, "<-")) != NULL)             //if found an assignment sign
    {
        memcpy(returnValue, string, start - string - 1);    //duplicate the return value bit
        returnValue[start - string - 1] = 0;                //NULL-terminate it
        strtok(returnValue, " \t");                         //remove trailing spaces
        start += strlen("<-");                              //skip assignment symbol
        while (strchr(" \t", *start) != NULL)
        {                                                   //find next non-whitespace
            start++;
        }
        bReturnValue = TRUE;
    }
    else
    {
        start = string;
        bReturnValue = FALSE;
    }
    //search the function dispatch table
    for (index = 0; etgFunctionTable[index].name != NULL; index++)
    {
        strLength = etgParseNamesMatch(start, etgFunctionTable[index].name);
        if (strLength > 0)                                  //if found a match
        {
            opcode->opcode = EOP_Function;
            opcode->function = etgFunctionTable[index].function;
            opcode->passThis = (bool32)etgFunctionTable[index].passThis;
            opcode->returnValue = MINUS1;
            opcode->nParameters = 0;
            //isolate and set the parameter template
            params = etgParametersIsolate(start, strLength);
            param = strtok(params, ETG_TokenDelimiters);
            varIndex = 0;
            while (param != NULL)
            {
#if ETG_ERROR_CHECKING
                if (varIndex >= etgFunctionTable[index].nParams && varIndex > 0)
                {
                    etgLoadErrorf(ETG, "Function call exceeds %d parameters.", etgFunctionTable[index].nParams);
                    return(FALSE);
                }
#endif
                if (etgIsNumber(param))
                {                                           //passing a constant
                    if (etgFunctionTable[index].type[varIndex] == EVT_Float)
                    {                                       //determine how to scan
                        parser = "%f";
                    }
                    else
                    {
                        parser = "%d";
                    }
                    nScanned = sscanf(param, parser, &constant);
#if ETG_ERROR_CHECKING
                    if (nScanned != 1)
                    {
                        etgLoadErrorf(ETG, "Bad format for constant '%s'.", param);
                        return(FALSE);
                    }
#endif
                    opcode->parameter[varIndex].type = EVT_Constant;
                    opcode->parameter[varIndex].param = constant;
                }
                else
                {
                    //get variable
                    variable = etgVariableFind(param);
#if ETG_ERROR_CHECKING
                    if (variable == NULL)
                    {
                        etgLoadErrorf(ETG, "Variable or label '%s' not found.", param);
                        return(FALSE);
                    }
//                  if (variable->type == EVT_Label)
//                  {
//                      etgLoadErrorf(ETG, "Cannot pass label '%s' to function.", param);
//                  }
                    if (variable->type == EVT_Pointer)
                    {
                        etgLoadErrorf(ETG, "!!!Cannot do pointer types yet.");
                        return(FALSE);
                    }
                    if (variable->type != etgFunctionTable[index].type[varIndex])
                    {
                        etgLoadErrorf(ETG, "Incompatable type for parameter %d. (%d != %d)", varIndex, variable->type, etgFunctionTable[index].type[varIndex]);
                        return(FALSE);
                    }
#endif //ETG_ERROR_CHECKING
                    opcode->parameter[varIndex].type = etgFunctionTable[index].type[varIndex];
                    switch (variable->type)
                    {                                               //adjust opcode based on var type
#if ETG_ERROR_CHECKING
                        case EVT_Label:
                            etgLoadErrorf(ETG, "Cannot use label '%s' as a function parameter.", param);
                            return(FALSE);
                            break;
#endif
                        case EVT_ConstLabel:
                        case EVT_VarLabel:
                            offset = variable->initial;
                            break;                                  //assigning a constant
                        default:
                            offset = variable->offset;
                            break;
                    }
                    opcode->parameter[varIndex].param = offset;
                }
                varIndex++;
                param = strtok(NULL, ETG_TokenDelimiters);
            }
            opcode->nParameters = varIndex;
#if ETG_ERROR_CHECKING
            if (varIndex != etgFunctionTable[index].nParams)
            {
                if ((etgFunctionTable[index].nParams & ETG_VariableParams) &&
                    varIndex >= etgFunctionTable[index].nParams)
                {
                    ;
                }
                else
                {
                    etgLoadErrorf(ETG, "Function call to '%s' has wrong number of parameters.  Expected %d found %d.",
                              etgFunctionTable[index].name, etgFunctionTable[index].nParams, varIndex);
                    return(FALSE);
                }
            }
#endif
            //figure out and set the return value
            if (bReturnValue)
            {                                               //if there is a return expected
#if ETG_ERROR_CHECKING
                if (etgFunctionTable[index].returnType == EVT_Void)
                {                                           //if there is no return value to be had
                    etgLoadErrorf(ETG, "No return value avaiable for '%s'", start);
                    return(FALSE);
                }
#endif
                variable = etgVariableFind(returnValue);
#if ETG_ERROR_CHECKING
                if (variable == NULL)
                {
                    etgLoadErrorf(ETG, "Variable '%s' not found.", returnValue);
                    return(FALSE);
                }
                if (variable->type == EVT_Label)
                {
                    etgLoadErrorf(ETG, "Cannot use label '%s' as return variable.", returnValue);
                    return(FALSE);
                }
                if (variable->type == EVT_Pointer)
                {
                    etgLoadErrorf(ETG, "!!!Cannot do pointer types yet.");
                    return(FALSE);
                }
#endif
                switch (variable->type)
                {                                               //adjust opcode based on var type
#if ETG_ERROR_CHECKING
                    case EVT_Label:
                        etgLoadErrorf(ETG, "Cannot use label '%s' as a function parameter.", returnValue);
                        return(FALSE);
                        break;
#endif
                    case EVT_ConstLabel:
                    case EVT_VarLabel:
                        offset = variable->initial;
                        break;                                  //assigning a constant
                    default:
                        offset = variable->offset;
                        break;
                }
                opcode->returnValue = offset;
            }
            //compute length of opcode
            *destLength = sizeof(etgfunctioncall) + sizeof(memsize) * 2 * (varIndex - 1);
            if (etgFunctionTable[index].resolve != NULL)    //if there is a resolve-function
            {
                etgFunctionTable[index].resolve(stat, opcode);    //call the resolve function
            }
            return(TRUE);                                   //finished
        }
    }
    return(FALSE);                                          //no matching function found
}

/*-----------------------------------------------------------------------------
    Name        : etgNestFunctionSet
    Description : Sets a function to be called at the closing of a brace nesting
    Inputs      : openFunction - function to call when a '{' is encountered, (optional)
                  function - function to call when closing brace encountered
                  code block - current code block (EPM_Startup etc.)
                  offset - current offset in that code block
                  userData - pointer parameter to be passed to callback
    Outputs     : stores these parameters in the current code nesting info
                    structure but does not increment etgNestLevel.
    Return      :
    Note        : usually in response to a conditional statement
----------------------------------------------------------------------------*/
void etgNestFunctionSet(nestopenfunction openFunction, nestfunction function, sdword codeBlock, sdword offset, ubyte *userData)
{
    //dbgAssertOrIgnore(etgNestStack[etgNestLevel].function == NULL);
#if ETG_ERROR_CHECKING
    if (etgNestLevel >= ETG_NestStackDepth)
    {
        etgLoadErrorf(ETG, "Nested %d levels of braces - too many.", ETG_NestStackDepth);
        return;
    }
#endif
    etgNestStack[etgNestLevel].openFunction = openFunction;
    etgNestStack[etgNestLevel].function = function;
    etgNestStack[etgNestLevel].codeBlock = codeBlock;
    etgNestStack[etgNestLevel].offset = offset;
    etgNestStack[etgNestLevel].userData = userData;
}

/*-----------------------------------------------------------------------------
    Name        : etgNestPush
    Description : Increments etgNestLevel in response to an open brace
    Inputs      : void
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void etgNestPush(void)
{
#if ETG_ERROR_CHECKING
    if (etgNestLevel >= ETG_NestStackDepth)
    {
        etgLoadErrorf(ETG, "Nested %d levels of braces - too many.", ETG_NestStackDepth);
        return;
    }
#endif
    etgNestLevel++;
    if (etgNestStack[etgNestLevel - 1].openFunction != NULL)
    {
        etgNestStack[etgNestLevel - 1].openFunction(etgNestStack[etgNestLevel - 1].codeBlock,
            etgNestStack[etgNestLevel - 1].offset, etgNestStack[etgNestLevel - 1].userData);
        etgNestStack[etgNestLevel - 1].openFunction = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : etgNestPop
    Description : Decrements etgNestLevel and calls the previously-set nesing function.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void etgNestPop(sdword newOffset)
{
#if ETG_ERROR_CHECKING
    if (etgNestLevel < 1)
    {
        etgLoadError(ETG, "More '}' than '{'.");
        return;
    }
#endif
    etgNestLevel--;                                         //decrement the nesting level
    if (etgNestStack[etgNestLevel].function != NULL)
    {                                                       //call the function
        etgNestStack[etgNestLevel].function(etgNestStack[etgNestLevel].codeBlock,
            etgNestStack[etgNestLevel].offset, newOffset, etgNestStack[etgNestLevel].userData);
        etgNestStack[etgNestLevel].function = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : etgNewVariableCreate
    Description : Create a new variable in the effect structure.
    Inputs      : name - name of variable
                  stat - effect we're working on
                  type - type of varible
                  setInitial - flag indicating initial value needs setting
                  bInitial - initial value
                  size - length of pointer value to set
    Outputs     : fills in the next entry in the variable name table
    Return      : length of name table
----------------------------------------------------------------------------*/
sdword etgNewVariableCreate(char *name, etgeffectstatic *stat, sdword type, bool32 bInitial, memsize initial, sdword size)
{
#if ETG_ERROR_CHECKING
    sdword index;

    for (index = 0; index < etgVariableIndex; index++)
    {
        if (strcmp(etgVariableTable[index].name, name) == 0)
        {
            dbgMessagef(ETG, "Redefinition of symbol '%s'.", name);
        }
    }
    if (etgVariableIndex >= ETG_VarTableParseLength)
    {
        etgLoadErrorf(ETG, "Out of space in variable name table: %d entries", ETG_VarTableParseLength);
        return(0);
    }
#endif
    etgVariableTable[etgVariableIndex].name = memStringDupe(name);
    etgVariableTable[etgVariableIndex].offset = stat->variableSize;
    etgVariableTable[etgVariableIndex].rateOffset = 0xffffffff;
    etgVariableTable[etgVariableIndex].initial = initial;
    etgVariableTable[etgVariableIndex].bInitial = (ubyte)bInitial;
    etgVariableTable[etgVariableIndex].type = (ubyte)type;
    etgVariableTable[etgVariableIndex].size = (ubyte)size;
    etgVariableTable[etgVariableIndex].codeBlock = (ubyte)etgParseMode;
    etgVariableIndex++;
    if (type != EVT_Label && type != EVT_ConstLabel && type != EVT_VarLabel)
    {
        stat->variableSize += sizeof(memsize);
    }
    return(etgVariableIndex);
}

/*-----------------------------------------------------------------------------
    Name        : etgVariableTableProcess
    Description : Process the variable table after the effect has been loaded
                    properly.
    Inputs      : void
    Outputs     : Frees all the strings in the table but not before creating a
                    variable initialization table with the following format:
                  destIndices[n] is index of variable to initialize with *destDataInit[n]
    Return      : number of initialization variables
----------------------------------------------------------------------------*/
sdword etgVariableTableProcess(ubyte **destOffsets, memsize **destInitData)
{
    memsize *tempBlock, *finalBlock;
    ubyte *tempOffsets, *finalOffsets;
    sdword tableIndex, index, varIndex;

    //create a temp block in the biggest size it could be
    tempBlock = memAlloc((sizeof(ubyte) + sizeof(memsize)) * etgVariableIndex, "TempVarInit", 0);
    tempOffsets = (ubyte *)(&tempBlock[etgVariableIndex]);
    for (tableIndex = index = varIndex = 0; index < etgVariableIndex; index++)
    {
        if (etgVariableTable[index].bInitial)
        {                                                   //if initialize this variable
            dbgAssertOrIgnore(tableIndex < etgVariableIndex);
            dbgAssertOrIgnore(index <= UDWORD_Max);
            tempOffsets[tableIndex] = (ubyte)varIndex;         //store variable index
            tempBlock[tableIndex] = etgVariableTable[index].initial;//store actual init data
            tableIndex++;
        }
        if (etgVariableTable[index].type != EVT_Label &&
            etgVariableTable[index].type != EVT_ConstLabel &&
            etgVariableTable[index].type != EVT_VarLabel)
        {
            varIndex++;
        }
        memFree(etgVariableTable[index].name);              //no name is needed anymore
    }
    if (tableIndex == 0)
    {                                                       //if no variables initialized
        memFree(tempBlock);                                 //free temporary memory
        *destInitData = NULL;
        *destOffsets = NULL;
        return(0);                                          //don't do nuttin'
    }
    finalBlock = memAlloc((sizeof(ubyte) + sizeof(memsize)) * tableIndex, "VarInitData", NonVolatile);
    finalOffsets = (ubyte *)(&finalBlock[tableIndex]);      //allocate the final blocks
    memcpy(finalBlock, tempBlock, sizeof(memsize) * tableIndex);//copy them over
    memcpy(finalOffsets, tempOffsets, sizeof(ubyte) * tableIndex);
    memFree(tempBlock);                                     //free temporary memory
    *destInitData = finalBlock;                             //return pointers
    *destOffsets = finalOffsets;
    return(tableIndex);
}

/*-----------------------------------------------------------------------------
    Name        : etgCodeBlockDistill
    Description : Resizes a code block to the minimum size required, then frees it.
    Inputs      : codeBlock - code block to resize
                  codeLength - current length of code block
                  newCode - where to store the new code block pointer
    Outputs     : newCode
    Return      : length of new code block code.
----------------------------------------------------------------------------*/
sdword etgCodeBlockDistill(ubyte *codeBlock, sdword codeLength, ubyte **newCode)
{
    if (codeLength == 0)
    {
        *newCode = NULL;
    }
    else
    {
        *newCode = memAlloc(codeLength, "EffectCodeBlock", NonVolatile);//alloc new block
        memcpy(*newCode, codeBlock, codeLength);            //copy old block over
    }
    memFree(codeBlock);                                     //free old block
    return(codeLength);                                     //length of new block
}

/*-----------------------------------------------------------------------------
    Name        : etgForwardReferenceFix
    Description : Fix all the forward references in a code block.
    Inputs      : codeSegment - segment to search through and fix
                  segmentLength - length of segment
    Outputs     : Any opcodes in the code segment with the EOP_Resolve in their
                    opcode will have their forward references fixed.
    Return      :
----------------------------------------------------------------------------*/
void etgForwardReferenceFix(ubyte *codeSegment, sdword segmentLength)
{
    sdword offset;
    etgopcode opcode;
    etgfunctioncall *call;
    etgbranch *branch;
    char *name;
    etgvarentry *label;

    for (offset = 0; offset < segmentLength;)
    {
        opcode = *((udword *)(codeSegment + offset));
        if (opcode & EOP_Resolve)
        {                                                   //if we have to update this opcode's branch member
            branch = (etgbranch *)(codeSegment + offset);
            name = (char *)branch->branchTo;                //get name
            label = etgVariableFind(name);                  //find the label
#if ETG_ERROR_CHECKING
            if (label == NULL)
            {
                etgLoadErrorf(ETG_U, "Unresolved branch destination '%s'", name);
                return;
            }
            if (label->type != EVT_Label)
            {
                etgLoadErrorf(ETG_U, "Symbol '%s' not a proper label name.", name);
                return;
            }
#endif
            branch->branchTo = label->initial;              //fix up the new offset
            branch->codeBlock = label->codeBlock;           //set code block
            memFree(name);                                  //free this no-longer needed name
            bitClear(branch->opcode, EOP_Resolve);          //clear the resolve bit
            bitClear(opcode, EOP_Resolve);
        }
        if (opcode == EOP_Function)
        {
            call = (etgfunctioncall *)(codeSegment + offset);
            offset += etgFunctionSize(call->nParameters);   //special offset update for function calls
        }
        else
        {
            offset += etgHandleTable[opcode].length;        //update the opcode pointer
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : etgEffectCodeLoad
    Description : Load in an effect from a .etg file.
    Inputs      : fileName - name of file to load
    Outputs     : allocates an effect static block and loads it in.
    Return      : newly allocated and loaded effect.
----------------------------------------------------------------------------*/
etgeffectstatic *etgEffectCodeLoad(char *fileName)
{
    char string[STRING_LENGTH];
    char *pString;
    etgeffectstatic *newStatic;
    ubyte tempOpcode[OPCODE_MAX];
    sdword opcodeLength, index, offset;

    //allocate and initialize the new effect static structure
    //newStatic = memAlloc(sizeof(etgeffectstatic), "EffectStatic", 0);
    //newStatic->name = NULL;                                 //no name yet
    newStatic = etgEffectStaticFind(fileName, TRUE);        //see if this effect already loaded
    if (newStatic->nParticleBlocks != 0xffffffff)
    {                                                       //if effect has already been loaded properly
        return(newStatic);                                  //return because we're done
    }
    if (etgNumberEffectsExpected > 0)
    {
        HorseRaceNext((real32)etgNumberEffectsLoaded / (real32)etgNumberEffectsExpected);
        etgNumberEffectsLoaded++;
    }
    newStatic->nParticleBlocks = ETG_NewParticleLength;     //default particle block size
    newStatic->nHistoryList = EH_DefaultSize;
    newStatic->variableSize = 0;                            //no variables yet
    newStatic->rateSize = 0;                                //no rate variables yet
    newStatic->softwareVersion = NULL;                      //by default, there is no software version

    //open the file
    dbgAssertOrIgnore(!etgFileHandle);
    etgFileHandle = fileOpen(fileName, FF_TextMode);        //open file
    etgFileLine = 0;                                        //start on first line
    etgTimeIndexVar = NULL;                                 //handle nesting errors OK
//    etgFileName = memStringDupe(etgLineRead(string, STRING_LENGTH));//first line of file is always name of source file
    etgFileName = memStringDupe(fileName);                  //save copy of file name
    etgParseMode = EPM_Nothing;                             //start parsing with nothing
    etgNestLevel = 0;                                       //start at base nesting level
    memset(etgNestStack, 0, sizeof(etgnestentry) * ETG_NestStackDepth);

#if ETG_VERBOSE_LEVEL >= 2
    dbgMessagef("etgEffectCodeLoad: '%s'", fileName);
#elif ETG_VERBOSE_LEVEL == 1
    dbgMessagef(".");
#endif
    //set up some variables, allocate memory
    etgExecStack.etgCodeBlock[EPM_Startup].offset = etgExecStack.etgCodeBlock[EPM_EachFrame].offset = etgExecStack.etgCodeBlock[EPM_TimeIndex].offset = 0;
    etgExecStack.etgCodeBlock[EPM_Startup].code = memAlloc(ETG_StartupParseSize, "StartupCodePool", 0);
    etgExecStack.etgCodeBlock[EPM_Startup].length = ETG_StartupParseSize;
    etgExecStack.etgCodeBlock[EPM_EachFrame].code = memAlloc(ETG_EachFrameParseSize, "EachFrameCodePool", 0);
    etgExecStack.etgCodeBlock[EPM_EachFrame].length = ETG_EachFrameParseSize;
    etgExecStack.etgCodeBlock[EPM_TimeIndex].code = memAlloc(ETG_TimeIndexParseSize, "TimeIndexCodePool", 0);
    etgExecStack.etgCodeBlock[EPM_TimeIndex].length = ETG_TimeIndexParseSize;
    etgVariableTable = memAlloc(sizeof(etgvarentry) * ETG_VarTableParseLength, "VariableNameBlock", 0);
    etgVariableIndex = 0;
    etgConstData = memAlloc(ETG_ConstDataPool, "ConstDataTemp", 0);
    newStatic->constLength = 0;
    newStatic->constData = NULL;
//    newStatic->bSelfDeleting = FALSE;
    newStatic->specialOps = 0;
    //set up for alternates
    etgAlternateTable = memAlloc(sizeof(etgextalt) * ETG_MaxAlternates, "LoadAltTable", 0);
    etgAlternateStructIndex = 0;
    etgTotalOffsets = 0;                                    //currently no decision offset tables

    while ((pString = etgLineRead(string, STRING_LENGTH)) != NULL)
    {
#if ETG_VERBOSE_LEVEL >= 3
        dbgMessagef("%s", pString);
#endif
        opcodeLength = 0;                                   //default to no opcode
        if (!etgOpcodeScan(newStatic, string, tempOpcode, &opcodeLength))//scan to see if there was an opcode
        {                                                   //if not a built-in opcode
            if (!etgFunctionCallScan(newStatic, string, tempOpcode, &opcodeLength))
            {                                               //or a defined function
                if (!etgVarCopyScan(newStatic, string, tempOpcode, &opcodeLength))
                {                                           //or even a variable assignment
#if ETG_ERROR_CHECKING
                    etgLoadErrorf(ETG, "Don't know how to handle '%s'", pString);
#endif
                }
            }
        }
        //set up an opcode structure in the appropriate code block
        if (opcodeLength > 0)
        {
#if ETG_ERROR_CHECKING
            if (opcodeLength > OPCODE_MAX)
            {
                etgLoadErrorf(ETG, "Opcode too long (%d > %d).", opcodeLength, OPCODE_MAX);
            }
            if (etgExecStack.etgCodeBlock[etgParseMode].offset + opcodeLength >= etgExecStack.etgCodeBlock[etgParseMode].length)
            {
                dbgFatalf(ETG, "Exceeded %d bytes of code in code block %d.",  etgExecStack.etgCodeBlock[etgParseMode].length, etgParseMode);
            }
#endif
            memcpy(etgExecStack.etgCodeBlock[etgParseMode].code + etgExecStack.etgCodeBlock[etgParseMode].offset,
                   tempOpcode, opcodeLength);
            etgExecStack.etgCodeBlock[etgParseMode].offset += opcodeLength;
        }
        if (PARSEERROR)
        {                                                   //if an error was encountered
            sdword j;
            //should free a bunch of temporary memory here
            for (j = 0; j < ETG_EventListLength; j++)
            {
                if (etgEventTable[j].effectStatic == newStatic)
                {
                    //since there are some things in this effect which may
                    //or may not have been allocated, let's just not free them.
                    //This is a debugging feature anyhow.
                    memFree(etgEventTable[j].effectStatic->name);
                    memFree(etgEventTable[j].effectStatic);
                    etgEventTable[j].effectStatic = NULL;
                    goto foundThisNewEffect;
                }
            }
            dbgFatalf(DBG_Loc, "Newly loaded effect '%s' not found in registry", newStatic->name);
foundThisNewEffect:;
            memFree(etgConstData);
            memFree(etgVariableTable);
            memFree(etgFileName);
            fileClose(etgFileHandle);                       //close the handle
            etgFileHandle = 0;
            etgErrorEncountered = FALSE;
            return(NULL);
        }
    }
    //fix all forward references in the code segments
    etgForwardReferenceFix(etgExecStack.etgCodeBlock[EPM_Startup].code, etgExecStack.etgCodeBlock[EPM_Startup].offset);
    etgForwardReferenceFix(etgExecStack.etgCodeBlock[EPM_EachFrame].code, etgExecStack.etgCodeBlock[EPM_EachFrame].offset);
    etgForwardReferenceFix(etgExecStack.etgCodeBlock[EPM_TimeIndex].code, etgExecStack.etgCodeBlock[EPM_TimeIndex].offset);

    //combine all the wastefully allocated alternate blocks
    if (etgAlternateStructIndex > 0)
    {
        newStatic->decisions = memAlloc(sizeof(etgalternate) * etgAlternateStructIndex, "DecideTable", NonVolatile);
        if (etgTotalOffsets > 0)                                //allocate memory
        {
            newStatic->alternateOffsets = memAlloc(sizeof(memsize) * etgTotalOffsets, "AltOffsetTable", NonVolatile);
        }
        else
        {
            newStatic->alternateOffsets = NULL;
        }
        for (index = offset = 0; index < etgAlternateStructIndex; index++)
        {
            pString = (char *)etgAlternateTable[index].decision->offset;
            memcpy(newStatic->alternateOffsets + offset, pString,//copy local offset table for this one
                   sizeof(memsize) * etgAlternateTable[index].decision->tableLength);
            etgAlternateTable[index].decision->offset = newStatic->alternateOffsets + offset;
            offset += etgAlternateTable[index].decision->tableLength;
            newStatic->decisions[index] = etgAlternateTable[index].alt;//copy just the needed part of the alternate offset table
            memFree(pString);                                   //free the old list
        }
        newStatic->nDecisions = etgAlternateStructIndex;  //remember how many alternates there are
    }
    else
    {
        newStatic->nDecisions = 0;
        newStatic->decisions = NULL;
        newStatic->alternateOffsets = NULL;
    }
    memFree(etgAlternateTable);                             //free the alternates table
    //free up the variable names and set variable init data members
    newStatic->initLength = etgVariableTableProcess(&newStatic->variableOffsets, &newStatic->variableInitData);
    dbgAssertOrIgnore(newStatic->variableSize + newStatic->rateSize >= 0);
    newStatic->effectSize = etgEffectSize(newStatic->nParticleBlocks) +
                newStatic->variableSize + newStatic->rateSize + newStatic->variableSize / 4;

    //create code blocks in proper size
    newStatic->codeBlock[EPM_Startup].length = etgCodeBlockDistill(etgExecStack.etgCodeBlock[EPM_Startup].code, etgExecStack.etgCodeBlock[EPM_Startup].offset, &newStatic->codeBlock[EPM_Startup].code);
    newStatic->codeBlock[EPM_EachFrame].length = etgCodeBlockDistill(etgExecStack.etgCodeBlock[EPM_EachFrame].code, etgExecStack.etgCodeBlock[EPM_EachFrame].offset, &newStatic->codeBlock[EPM_EachFrame].code);
    newStatic->codeBlock[EPM_TimeIndex].length = etgCodeBlockDistill(etgExecStack.etgCodeBlock[EPM_TimeIndex].code, etgExecStack.etgCodeBlock[EPM_TimeIndex].offset, &newStatic->codeBlock[EPM_TimeIndex].code);

    //distill the constant data
    if (newStatic->constLength > 0)
    {                                                       //if anything was allocated
        newStatic->constData = memAlloc(newStatic->constLength, "ConstData", NonVolatile);
        memcpy(newStatic->constData, etgConstData, newStatic->constLength);
    }
    //create a history list, if applicable
    if (newStatic->nHistoryList > 0)
    {
        newStatic->iHistoryList = 0;
        newStatic->historyList = memAlloc(sizeof(real32) * newStatic->nHistoryList, "ETGHistoryList", NonVolatile);
        for (index = 0; index < newStatic->nHistoryList; index++)
        {
            newStatic->historyList[index] = REALlyNegative;
        }
    }
    else
    {
        newStatic->historyList = NULL;
    }
    //free some memory
    memFree(etgConstData);
    memFree(etgVariableTable);
    memFree(etgFileName);
    fileClose(etgFileHandle);                               //close the handle
    etgFileHandle = 0;
    return(newStatic);                                      //all done
}

/*-----------------------------------------------------------------------------
    Effect parsing functions:
-----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        :
    Description : Opcode parsing functions have the following format
    Inputs      : dest - where to store the new opcode structure, if any
                  opcode - string representing the opcode-string
                  params - string representing the parameters (or rest of the line)
                  ret - string representing the parameter to assign return value to
    Outputs     : Creates a new opcode of some format and stores it at dest.
    Return      : Amount to update the code handle by, if any.
----------------------------------------------------------------------------*/
sdword etgLineNumberUpdate(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    char string[STRING_LENGTH];
    sdword nScanned, lineNumber;

    nScanned = sscanf(params, "%d \"%s\"", &lineNumber, string);
#if ETG_ERROR_CHECKING
    if (nScanned != 2 || lineNumber < 1 || strlen(string) < 1)
    {
        etgLoadErrorf(ETG, "Invalid '#line' directive '%s'.", params);
    }
#endif
    etgFileLine = lineNumber;                               //set current line number
    memFree(etgFileName);                                   //free old name
    etgFileName = memStringDupe(string);                    //all done
    return(0);
}

sdword etgNOPCreate(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    etgnop *nop = (etgnop *)dest;
    nop->opcode = EOP_Nop;
    return(sizeof(etgnop));
}
//handle the '{'
sdword etgBlockOpen(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    etgNestPush();
    return(0);
}
//handle the '}'
sdword etgBlockClose(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    sdword offset = -1;
    if (etgParseMode < ETG_NumberCodeBlocks)
    {
        offset = etgExecStack.etgCodeBlock[etgParseMode].offset;
    }
    etgNestPop(offset);
    return(0);
}

//callback for handing the '}' of the master block levels
void etgParseModeRestore(sdword codeBlock, sdword offset, sdword newOffset, ubyte *userData)
{
    etgParseMode = codeBlock;
}

//callback for handing '}' to insert an 'end' opcode in the code block
void etgCodeBlockEnd(sdword codeBlock, sdword off, sdword newOffset, ubyte *userData)
{
    sdword *offset;
    etgprocesscontrol *opcode;

    dbgAssertOrIgnore(etgParseMode >= 0 && etgParseMode < ETG_NumberCodeBlocks);
    offset = (sdword *)&etgExecStack.etgCodeBlock[etgParseMode].offset;
    opcode = (etgprocesscontrol *)(etgExecStack.etgCodeBlock[etgParseMode].code + *offset);
    opcode->opcode = EOP_End;                               //create the opcode
    *offset += sizeof(etgprocesscontrol);                   //and update pointer
    etgParseMode = codeBlock;                               //and finally restore the parsing mode of before
    etgTimeIndexIndex = 0;                                  //in case we're ending a time index block, reset this counter
}

//handle opening the variable block
sdword etgVariableBlockOpen(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
#if ETG_ERROR_CHECKING
    if (etgParseMode != EPM_Nothing || etgNestLevel != 0)
    {
        dbgMessagef(ETG, "Starting variable block in nesting level %d", etgNestLevel);
    }
#endif
    etgNestFunctionSet(NULL, etgParseModeRestore, etgParseMode, 0, NULL);//set a callback
    etgParseMode = EPM_Variables;                           //we're now in variable land
    return(0);
}

//handle opening code blocks
sdword etgStartupBlockOpen(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    etgprocesscontrol *endcode;
#if ETG_ERROR_CHECKING
    if (etgParseMode != EPM_Nothing || etgNestLevel != 0)
    {
        dbgMessagef(ETG, "Starting code block in nesting level %d", etgNestLevel);
    }
#endif
    if (etgExecStack.etgCodeBlock[EPM_Startup].offset != 0)
    {                                                       //if not the first such code block
        endcode = (etgprocesscontrol *)(etgExecStack.etgCodeBlock[EPM_Startup].code + etgExecStack.etgCodeBlock[EPM_Startup].offset - sizeof(etgprocesscontrol));
        dbgAssertOrIgnore((ubyte *)endcode >= etgExecStack.etgCodeBlock[EPM_Startup].code);
        dbgAssertOrIgnore(endcode->opcode == EOP_End);
        etgExecStack.etgCodeBlock[EPM_Startup].offset -= sizeof(etgprocesscontrol);//remove the previous end opcode
    }
    etgNestFunctionSet(NULL, etgCodeBlockEnd, etgParseMode, 0, NULL);//set a callback
    etgParseMode = EPM_Startup;                             //we're now in some code block
    return(0);
}
sdword etgEachFrameBlockOpen(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    etgprocesscontrol *endcode;
#if ETG_ERROR_CHECKING
    if (etgParseMode != EPM_Nothing || etgNestLevel != 0)
    {
        dbgWarningf(ETG, "Starting code block in nesting level %d", etgNestLevel);
    }
#endif
    if (etgExecStack.etgCodeBlock[EPM_EachFrame].offset != 0)
    {                                                       //if not the first such code block
        endcode = (etgprocesscontrol *)(etgExecStack.etgCodeBlock[EPM_EachFrame].code + etgExecStack.etgCodeBlock[EPM_EachFrame].offset - sizeof(etgprocesscontrol));
        dbgAssertOrIgnore((ubyte *)endcode >= etgExecStack.etgCodeBlock[EPM_EachFrame].code);
        dbgAssertOrIgnore(endcode->opcode == EOP_End);
        etgExecStack.etgCodeBlock[EPM_EachFrame].offset -= sizeof(etgprocesscontrol);//remove the previous end opcode
    }
    etgNestFunctionSet(NULL, etgCodeBlockEnd, etgParseMode, 0, NULL);//set a callback
    etgParseMode = EPM_EachFrame;                             //we're now in some code block
    return(0);
}
sdword etgTimeIndexBlockOpen(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    etgprocesscontrol *endcode;
#if ETG_ERROR_CHECKING
    if (etgParseMode != EPM_Nothing || etgNestLevel != 0)
    {
        dbgMessagef(ETG, "Starting code block in nesting level %d", etgNestLevel);
    }
#endif
    if (etgExecStack.etgCodeBlock[EPM_TimeIndex].offset != 0)
    {                                                       //if not the first such code block
        endcode = (etgprocesscontrol *)(etgExecStack.etgCodeBlock[EPM_TimeIndex].code + etgExecStack.etgCodeBlock[EPM_TimeIndex].offset - sizeof(etgprocesscontrol));
        dbgAssertOrIgnore((ubyte *)endcode >= etgExecStack.etgCodeBlock[EPM_TimeIndex].code);
        dbgAssertOrIgnore(endcode->opcode == EOP_End);
        etgExecStack.etgCodeBlock[EPM_TimeIndex].offset -= sizeof(etgprocesscontrol);//remove the previous end opcode
    }
    etgNestFunctionSet(NULL, etgCodeBlockEnd, etgParseMode, 0, NULL);//set a callback
    etgParseMode = EPM_TimeIndex;                             //we're now in some code block
    return(0);
}

sdword etgWorldRenderSet(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    bitSet(stat->specialOps, ESO_WorldRender);
    return(0);
}
/*
sdword etgForceVisibleSet(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    bitSet(stat->specialOps, ESO_ForceVisible);
    return(0);
}
*/
/*-----------------------------------------------------------------------------
    Name        : etgParseVariable
    Description : service function for parsing help in variable definitions
    Inputs      : params - pointer to something like "velocity = 35"
    Outputs     : splits the assignment and the variable name apart by null-
                    terminating the variable name.  params would become
                    just "velocity".
    Return      : pointer to assignment value (35 in above example), or NULL
                    if no initial value.
----------------------------------------------------------------------------*/
char *etgParseVariable(char *params, bool32 *bSetInitial)
{
    char *start, *end;
    if ((start = end = strchr(params, '=')) == NULL)
    {                                                       //if no initial value
        *bSetInitial = FALSE;
        return(NULL);
    }
    else
    {
        *bSetInitial = TRUE;
        start++;                                            //skip the '='
        while (strchr(" \t", *start))
        {                                                   //skip leading whitespace
            start++;
        }
    }
    end--;
    while (strchr(" \t", *end))                             //find end of variable name
    {
        end--;
    }
    *(end + 1) = 0;                                         //NULL-terminate end of name
#if ETG_ERROR_CHECKING
    if (strlen(params) <= 1)
    {
        etgLoadErrorf(ETG, "syntax error defining variable");
    }
#endif
    return(start);
}

//create a new integer
sdword etgNewInteger(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    memsize initial;
    sdword nScanned;
    bool32 bSetInitial;
    char *start;

    if (etgParseMode == EPM_Constant)
    {                                                       //if we're creating a constant
        start = strtok(params, " =\t");
#ifdef HW_PTR_64
        nScanned = sscanf(start, "%ld", &initial);
#else
        nScanned = sscanf(start, "%d", &initial);
#endif
#if ETG_ERROR_CHECKING
        if (stat->constLength + 4 > ETG_ConstDataPool)
        {
            etgLoadErrorf(ETG, "Exceeded %d bytes of constant data.", ETG_ConstDataPool);
            return(0);
        }
        if (nScanned != 1)
        {
            etgLoadErrorf(ETG, "Error scanning '%s' for integer constant", params);
            return(0);
        }
#endif
        *((udword *)(etgConstData + stat->constLength)) = initial;
        stat->constLength += sizeof(memsize);
        return(0);
    }
#if ETG_ERROR_CHECKING                                      //else it'a an actual variable
    if (etgParseMode != EPM_Variables)
    {
        etgLoadErrorf(ETG, "Variable definition outside of variable block.");
        return(0);
    }
#endif
    start = etgParseVariable(params, &bSetInitial);         //prepare string
    if (bSetInitial)
    {
#ifdef HW_PTR_64
        nScanned = sscanf(start, "%ld", &initial);           //scan the initial value
#else
        nScanned = sscanf(start, "%d", &initial);           //scan the initial value
#endif
#if ETG_ERROR_CHECKING
        if (nScanned != 1)
        {
            etgLoadErrorf(ETG, "syntax error in variable initialization: '%s'", start);
            return(0);
        }
#endif
    }                                                       //create new variable
    etgNewVariableCreate(params, stat, EVT_Int, bSetInitial, initial, 0);
    return(0);
}

//create a new RGB variable
sdword etgNewRGB(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    udword initial;
    bool32 bSetInitial;
    char *start;

#if ETG_ERROR_CHECKING
    if (etgParseMode != EPM_Variables)
    {
        etgLoadErrorf(ETG, "Variable definition outside of variable block.");
        return(0);
    }
#endif
    start = etgParseVariable(params, &bSetInitial);         //prepare string
    if (bSetInitial)
    {
        initial = etgColorScan(start);
    }                                                       //create new variable
    etgNewVariableCreate(params, stat, EVT_RGB, bSetInitial, initial, 0);
    return(0);
}

//create a new RGBA variable
sdword etgNewRGBA(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    udword initial;
    bool32 bSetInitial;
    char *start;

#if ETG_ERROR_CHECKING
    if (etgParseMode != EPM_Variables)
    {
        etgLoadErrorf(ETG, "Variable definition outside of variable block.");
        return(0);
    }
#endif
    start = etgParseVariable(params, &bSetInitial);         //prepare string
    if (bSetInitial)
    {
        initial = etgColorScan(start);
    }                                                       //create new variable
    etgNewVariableCreate(params, stat, EVT_RGBA, bSetInitial, initial, 0);
    return(0);
}

//create a new texture anim frame
sdword etgSubTextureDWORD(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    char *texName, *string;
    udword nScanned, xyInfo, flags;

    texName = strtok(params, " \t,");
#if ETG_ERROR_CHECKING
    if (texName == NULL)
    {
        etgLoadErrorf(ETG, "Error reading animation frame info.");
        return(0);
    }
#endif
    string = strtok(NULL, " \t,");
#if ETG_ERROR_CHECKING
    if (string == NULL)
    {
        etgLoadErrorf(ETG, "Error reading animation frame info.");
        return(0);
    }
#endif
    nScanned = sscanf(string, "%d", &xyInfo);
#if ETG_ERROR_CHECKING
    if (nScanned != 1)
    {
        etgLoadErrorf(ETG, "Error reading animation frame info.");
        return(0);
    }
#endif
    string = strtok(NULL, " \t,");
#if ETG_ERROR_CHECKING
    if (string == NULL)
    {
        etgLoadErrorf(ETG, "Error reading animation frame info.");
        return(0);
    }
#endif
    nScanned = sscanf(string, "%d", &flags);
#if ETG_ERROR_CHECKING
    if (nScanned != 1)
    {
        etgLoadErrorf(ETG, "Error reading animation frame info.");
        return(0);
    }
    if (stat->constLength + SCALESHIFT(12) > ETG_ConstDataPool)
    {
        etgLoadErrorf(ETG, "Exceeded %d bytes of constant data.", ETG_ConstDataPool);
    }
#endif
    *((trhandle *)(etgConstData + stat->constLength)) = trTextureRegister(texName, NULL, stat, TRUE);  // CAT: 4th arg 1 (etgSubTextureDWORD 0x0046caf0)
    stat->constLength += sizeof(trhandle);
    *((memsize *)(etgConstData + stat->constLength)) = SCALECAST(xyInfo);
    stat->constLength += sizeof(memsize);
    *((memsize *)(etgConstData + stat->constLength)) = SCALECAST(flags);
    stat->constLength += sizeof(memsize);
    return(0);
}

//create a new mesh-morph anim frame
sdword etgMorphAnimDWORD(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    char *meshName, *string;
    udword nScanned, flags;

    meshName = strtok(params, " \t,");
#if ETG_ERROR_CHECKING
    if (meshName == NULL)
    {
        etgLoadErrorf(ETG, "Error reading morph frame info.");
        return(0);
    }
#endif
    string = strtok(NULL, " \t,");
#if ETG_ERROR_CHECKING
    if (string == NULL)
    {
        etgLoadErrorf(ETG, "Error reading morph frame info.");
        return(0);
    }
#endif
    nScanned = sscanf(string, "%d", &flags);
#if ETG_ERROR_CHECKING
    if (nScanned != 1)
    {
        etgLoadErrorf(ETG, "Error reading morph frame info.");
        return(0);
    }
    if (stat->constLength + SCALESHIFT(8) > ETG_ConstDataPool)
    {
        etgLoadErrorf(ETG, "Exceeded %d bytes of constant data.", ETG_ConstDataPool);
    }
#endif
    *((meshdata **)(etgConstData + stat->constLength)) = etgMeshRegister(meshName);
    stat->constLength += sizeof(meshdata *);
//    *((udword *)(etgConstData + stat->constLength)) = xyInfo;
//    stat->constLength += sizeof(udword);
    *((memsize *)(etgConstData + stat->constLength)) = SCALECAST(flags);
    stat->constLength += sizeof(memsize);
    return(0);
}

//handle setting the texture handle for a sprite.  Creates a special function call
sdword etgSetTextureParse(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    etgfunctioncall *function = (etgfunctioncall *)dest;
    char path[PATH_MAX];
    char *param;
    etgvarentry *lVar;
    sdword index, nScanned;

    param = strtok(params, ETG_TokenDelimiters);
#if ETG_ERROR_CHECKING
    if (param == NULL)
    {
        etgLoadErrorf(ETG, "'%s' syntax: setTexture(<texName>, x, y, x', y')", opcode);
        return(0);
    }
#endif
    if (strcasecmp(opcode, "setTexture") == 0)
    {
        function->function = partSetTrHandle;                 // CAT: an argument-array callable
    }
    else
    {
        lVar = etgVariableFind(param);                     //find the variable
#if ETG_ERROR_CHECKING
        if (lVar == NULL)
        {
            etgLoadErrorf(ETG, "Variable '%s' not found.", param);
            return(0);
        }
        if (lVar->type != EVT_Int)
        {
            etgLoadErrorf(ETG, "'%s' needs integer handle.", opcode);
            return(0);
        }
        if (lVar->type == EVT_Pointer)
        {
            etgLoadErrorf(ETG, "!!!Cannot do pointer types yet.");
            return(0);
        }
#endif
        function->parameter[function->nParameters].type = EVT_Int;//first param is the texture handle
        function->parameter[function->nParameters].param = lVar->offset;
        function->function = partModifyTexture;                 // CAT: an argument-array callable
        function->nParameters++;
        param = strtok(NULL, ETG_TokenDelimiters);
    }
#if ETG_ERROR_CHECKING
    if (param == NULL)
    {
        etgLoadErrorf(ETG, "'%s' syntax: setTexture(<texName>, x, y, x', y')", opcode);
        return(0);
    }
#endif
    strcpy(path, ETG_Directory);
    strcat(path, param);
    function->opcode = EOP_Function;
    function->nParameters = 0;
    //function->nParameters = 1;  //!!!
    //function->nParameters = 5
    function->returnValue = MINUS1;
    function->passThis = FALSE;
    function->parameter[function->nParameters].type = EVT_Constant;                 //first param is the texture handle
    function->parameter[function->nParameters].param = trTextureRegister(path, NULL, dest, TRUE);  // CAT: 4th arg 1 (etgSetTextureParse 0x0046cc60)//a dummy dest should prevent it from sorting with other ships
    function->nParameters++;


    for (index = 1; index <= 4; index++)
    {
        param = strtok(NULL, ETG_TokenDelimiters);
#if ETG_ERROR_CHECKING
        if (param == NULL)
        {
            etgLoadErrorf(ETG, "'%s' syntax: setTexture(<texName>, x, y, x', y').  Missing parameter %d", opcode, index + 1);
            return(0);
        }
#endif
        function->parameter[function->nParameters].type = EVT_Constant;
#ifdef HW_PTR_64
        nScanned = sscanf(param, "%ld", &function->parameter[function->nParameters].param);
#else
        nScanned = sscanf(param, "%d", &function->parameter[function->nParameters].param);
#endif
#if ETG_ERROR_CHECKING
        if (nScanned != 1)
        {
            etgLoadErrorf(ETG, "'%s' bad format for parameter %d", opcode, index + 1);
            return(0);
        }
#endif
        function->nParameters++;
    }

    return(etgFunctionSize(function->nParameters));
}

/*-----------------------------------------------------------------------------
    Name        : etgMeshRegister
    Description : Load in a mesh if it's not already loaded
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
meshdata *etgMeshRegister(char *filename)
{
    sdword index;
    etgmeshreg *free = NULL;

    for (index = 0; index < ETG_NumberMeshes; index++)
    {                                                       //scan through the registry
        if (etgMeshRegistry[index].filename == NULL)
        {                                                   //if this guy free
            if (free == NULL)
            {                                               //record pointer to a free one
                free = &etgMeshRegistry[index];
            }
        }
        else
        {                                                   //else something loaded here
            if (!strcasecmp(filename, etgMeshRegistry[index].filename))
            {                                               //if this is the mesh we want
                return(etgMeshRegistry[index].mesh);
            }
        }
    }
    //if it gets here, no mesh was found, load it in.
#if ETG_ERROR_CHECKING
    if (free == NULL)
    {                                                       //if no free spots left
        dbgFatalf(DBG_Loc, "All %d slots in mesh registry used when loading '%s'", ETG_NumberMeshes, filename);
    }
#endif
    free->filename = memStringDupe(filename);               // CAT: memStringDupe (etgSetMeshParse 0x0046cdf0, etgMorphAnimDWORD 0x0046cba0), HW1 memStringDupeNV
    free->mesh = meshLoad(filename);
    return(free->mesh);
}

/*-----------------------------------------------------------------------------
    Name        : etgMeshRegistryReset
    Description : Clear out any meshes loaded into the mesh registry.
    Inputs      : void
    Outputs     : Frees all texture loaded into to mesh registry and set the
                    registry pointers to NULL.
    Return      : void
    Note        : No effects should be loaded when this is called as they
                    reference the data to be freed by this function.  Also,
                    this should not be called at startup of the game.
----------------------------------------------------------------------------*/
void etgMeshRegistryReset(void)
{
    sdword index;

    for (index = 0; index < ETG_NumberMeshes; index++)
    {                                                       //scan through the registry
        if (etgMeshRegistry[index].filename != NULL)
        {                                                   //if this guy has something allocated
#if ETG_VERBOSE_LEVEL >= 1
            dbgMessagef("etgMeshRegistryClear: freeing '%s' at 0x%x", etgMeshRegistry[index].filename, etgMeshRegistry[index].mesh);
#endif
            memFree(etgMeshRegistry[index].filename);
            etgMeshRegistry[index].filename = NULL;
            meshFree(etgMeshRegistry[index].mesh);
            etgMeshRegistry[index].mesh = NULL;
        }
    }
}

//handle 'setMesh'
sdword etgSetMeshParse(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    etgfunctioncall *function = (etgfunctioncall *)dest;
    char *param;

    param = strtok(params, ETG_TokenDelimiters);
#if ETG_ERROR_CHECKING
    if (param == NULL)
    {
        etgLoadErrorf(ETG, "Syntax: %s(<meshName>)", opcode);
        return(0);
    }
/*
    if (etgMeshRegistryIndex >= ETG_NumberMeshes)
    {
        etgLoadErrorf(ETG, "Exceeded %d ETG meshes.", ETG_NumberMeshes);
    }
*/
#endif
    function->opcode = EOP_Function;
    function->function = partSetMeshdata;                 // CAT: an argument-array callable
    function->nParameters = 1;
    function->returnValue = MINUS1;
    function->passThis = FALSE;
    //!!! perhaps we can register these instead of just loading them?
//    etgMeshRegistry[etgMeshRegistryIndex] = meshLoad(param);//load mesh into mesh registry
    function->parameter[0].type = EVT_Constant;             //and store pointer
//    function->parameter[0].param = (udword)etgMeshRegistry[etgMeshRegistryIndex];
    function->parameter[0].param = (memsize)etgMeshRegister(param);
//    etgMeshRegistryIndex++;                                 //update mesh registry index

    return(etgFunctionSize(1));
}

//create a new float
sdword etgNewFloat(struct etgeffectstatic *stat, ubyte *dest, char *opcode, char *params, char *ret)
{
    udword initial;
    sdword nScanned;
    bool32 bSetInitial;
    char *start;

    if (etgParseMode == EPM_Constant)
    {                                                       //if we're creating a constant
        start = strtok(params, " =\t");
        nScanned = sscanf(start, "%f", (float*)&initial);
#if ETG_ERROR_CHECKING
        if (stat->constLength + 4 > ETG_ConstDataPool)
        {
            etgLoadErrorf(ETG, "Exceeded %d bytes of constant data.", ETG_ConstDataPool);
            return(0);
        }
        if (nScanned != 1)
        {
            etgLoadErrorf(ETG, "Error scanning '%s' for float constant", params);
            return(0);
        }
#endif
        *((udword *)(etgConstData + stat->constLength)) = initial;
        stat->constLength += sizeof(memsize);
        return(0);
    }
#if ETG_ERROR_CHECKING
    if (etgParseMode != EPM_Variables)
    {
        etgLoadErrorf(ETG, "Variable definition outside of variable block.");
        return(0);
    }
#endif
    start = etgParseVariable(params, &bSetInitial);         //prepare string
    if (bSetInitial)
    {
        nScanned = sscanf(start, "%f", (float*)&initial);           //scan the initial value
#if ETG_ERROR_CHECKING
        if (nScanned != 1)
        {
            etgLoadErrorf(ETG, "syntax error in variable initialization: '%s'", start);
            return(0);
        }
#endif
    }                                                       //create new variable
    etgNewVariableCreate(params, stat, EVT_Float, bSetInitial, initial, 0);
    return(0);
}

/*-----------------------------------------------------------------------------
    Name        : etgConditionalComplete
    Description : Complete a conditional opcode in response to a '}'
    Inputs      : codeBlock: either startp, eachFrame or TimeIndex
                  offset: offset of the opcode to look for
                  newOffset: new code offset in code segment
    Outputs     : sets the codeBytes of the conditional opcode
    Return      :
----------------------------------------------------------------------------*/
etgconditional *etgIfOpcode;
void etgConditionalComplete(sdword codeBlock, sdword offset, sdword newOffset, ubyte *userData)
{
    etgconditional *opcode;

#if ETG_ERROR_CHECKING
    if (etgParseMode != codeBlock)
    {
        etgLoadErrorf(ETG, "closing '}' in different code block from the corresponding 'if'");
        return;
    }
    if (newOffset < offset)
    {
        etgLoadErrorf(ETG, "negative code bytes in 'if' code block");
        return;
    }
#endif
    //get opcode pointer
    opcode = (etgconditional *)(etgExecStack.etgCodeBlock[codeBlock].code + offset);
    //make sure we've found a good opcode
#if ETG_ERROR_CHECKING
    if (opcode->opcode < EOP_LowestConditional || opcode->opcode > EOP_HighestConsitional)
    {
        etgLoadErrorf(ETG, "Cannot find conditional opcode.  Found opcode %d instead", opcode->opcode);
        return;
    }
    dbgAssertOrIgnore(opcode->codeBytes == -1);
#endif
    //set code length in opcode
    opcode->codeBytes = newOffset - offset;
    etgIfOpcode = opcode;                               //save opcode reference
}

//handle 'if' statements
sdword etgConditional(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    char *param0, *oper, *param1, *tempparam;
    etgvarentry *var0, *var1, *tempvar;
    sdword isVar0, isVar1, bTranspose;
    char *parser;
    udword constant;
    etgconditional *opcode = (etgconditional *)dest;
    sdword index, subIndex;

#if ETG_ERROR_CHECKING
    if (etgParseMode != EPM_Startup && etgParseMode != EPM_EachFrame && etgParseMode != EPM_TimeIndex)
    {
        etgLoadError(ETG, "'if' conditional statement outside of code blocks.");
        return(0);
    }
#endif
    param0 = strtok(params, " \t");
#if ETG_ERROR_CHECKING
    if (param0 == NULL)
    {
        etgLoadError(ETG, "'if' conditional statement missing left parameter.");
        return(0);
    }
#endif
    oper = strtok(NULL, " \t");
#if ETG_ERROR_CHECKING
    if (oper == NULL)
    {
        etgLoadError(ETG, "'if' conditional statement missing operator.");
        return(0);
    }
#endif
    param1 = strtok(NULL, " \t");
#if ETG_ERROR_CHECKING
    if (param1 == NULL)
    {
        etgLoadError(ETG, "'if' conditional statement missing right parameter.");
        return(0);
    }
#endif
    //now that we've isolated the parameters, let's figure out the types and stuff
    if (!etgIsNumber(param0))
    {                                                       //if first parameter is a variable
        isVar0 = 1;
        var0 = etgVariableFind(param0);
    }
    else
    {
        isVar0 = 0;
    }
    if (!etgIsNumber(param1))
    {                                                       //if other parameter is a variable
        isVar1 = 1;
        var1 = etgVariableFind(param1);
    }
    else
    {
        isVar1 = 0;
    }
    //ensure they're in the right order
    if ((!isVar0) && isVar1)                                //if constant/variable
    {
        bTranspose = TRUE;                                  //need to use the transpose operator
        swapInt(isVar0, isVar1);                            //swap the operators
//        swapInt((udword)param0, (udword)param1);
        swap(param0, param1, tempparam);
        swap(var0, var1, tempvar);
    }
    else
    {
        bTranspose = FALSE;
    }
    //ensure the types are the same if they are both variables
    if (isVar0)
    {                                                       //if first one a variable
#if ETG_ERROR_CHECKING
        if (isVar1)
        {
            if (var0->type != EVT_Float && var1->type != EVT_Int)
            {                                               //if not a float or an int
                etgLoadErrorf(ETG, "Variable '%s' not a comparable type.");
                return(0);
            }
            if (var0->type != var1->type)
            {                                               //if differing types
                etgLoadErrorf(ETG, "Variables '%s' and '%s' are of different type", param0, param1);
                return(0);
            }
        }
#endif
    }
    else
    {
#if ETG_ERROR_CHECKING
        if (!isVar1)
        {                                                   //if both constants
            etgLoadErrorf(ETG, "'%s' and '%s' are both constants - use #if", param0, param1);
            return(0);
        }
#endif
    }

    if (var0->type == EVT_Float)
    {                                                       //choose appropriate sscanf format string
        parser = "%f";
    }
    else
    {
        parser = "%d";
    }
    if (!isVar1)
    {                                                       //if var1 constant
        sscanf(param1, parser, &constant);                  //scan in the constant
    }
    //now set the parameters in the compare opcode
    opcode->param0 = var0->offset;                          //set offset of this variable
    if (isVar1)
    {
        opcode->param1 = var1->offset;
    }
    else
    {
        opcode->param1 = constant;
    }
    //now let's find the appropriate opcode
    if (isVar1)
    {                                                       //if var-var
        if (var0->type == EVT_Float)
        {                                                   //var-var-float
            subIndex = 3;
        }
        else
        {                                                   //var-var-int
            subIndex = 1;
        }
    }
    else
    {
        if (var0->type == EVT_Float)
        {                                                   //var-const-float
            subIndex = 2;
        }
        else
        {                                                   //var-const-int
            subIndex = 0;
        }
    }
    //now we have everything but an operator string match.  Let's be doing that presently
    for (index = 0; strcmp(etgConditionalTable[index].symbol, "end"); index++)
    {
        if (strcmp(etgConditionalTable[index].symbol, oper) == 0)
        {                                                   //if names match
            if (bTranspose)
            {                                               //if we're supposed to use the transpose operator
                bTranspose = FALSE;                         //let's use it
                oper = etgConditionalTable[index].transpose;
                index = -1;
                continue;
            }
            opcode->opcode = etgConditionalTable[index].opcode[subIndex];
            break;                                          //set the opcode and stop looking
        }
    }
    //the only thing left is the codebytes member which must be set at the
    //  matching '}' line.  Let's prepare for this eventuality.
    dbgAssertOrIgnore(etgParseMode >= 0 && etgParseMode < ETG_NumberCodeBlocks);
    index = etgExecStack.etgCodeBlock[etgParseMode].offset;
    opcode->codeBytes = -1;                                 //flag code bytes not set
    etgNestFunctionSet(NULL, etgConditionalComplete, etgParseMode, index, (ubyte *)opcode);//set function to be called later
    return(sizeof(etgconditional));
}

//handle closing an 'else' block
void etgElseComplete(sdword codeBlock, sdword offset, sdword newOffset, ubyte *userData)
{
    etgbranch *opcode;
    etgconditional *ifOpcode;

#if ETG_ERROR_CHECKING
    if (etgParseMode != codeBlock)
    {
        etgLoadErrorf(ETG, "Closing '}' in different code block from the corresponding 'else'");
        return;
    }
    if (newOffset < offset)
    {
        etgLoadErrorf(ETG, "Negative code bytes in 'else' code block.");
        return;
    }
    if (newOffset == offset)
    {
        dbgWarningf(ETG, "Zero code bytes in 'else' code block.");
        return;
    }
#endif
    //get opcode pointers
    if (codeBlock == EPM_Startup)
    {
        opcode = (etgbranch *)(etgExecStack.etgCodeBlock[EPM_Startup].code + offset);
    }
    else if (codeBlock == EPM_EachFrame)
    {
        opcode = (etgbranch *)(etgExecStack.etgCodeBlock[EPM_EachFrame].code + offset);
    }
    else if (codeBlock == EPM_TimeIndex)
    {
        opcode = (etgbranch *)(etgExecStack.etgCodeBlock[EPM_TimeIndex].code + offset);
    }
    ifOpcode = (etgconditional *)userData;
    //make sure we've found a good opcode
    if (ifOpcode->opcode < EOP_EqualVCI || ifOpcode->opcode > EOP_NotZeroVF)
    {
        etgLoadErrorf(ETG, "'else' statement matches with an invalid 'if' (opcode %d)", ifOpcode->opcode);
        return;
    }
    ifOpcode->codeBytes += sizeof(etgbranch);
#if ETG_ERROR_CHECKING
    if (opcode->opcode != EOP_Goto)
    {
        etgLoadErrorf(ETG, "Cannot find else 'goto' opcode.  Found opcode %d instead.", opcode->opcode);
        return;
    }
    dbgAssertOrIgnore(opcode->branchTo == MINUS1);
#endif
    //set code length in opcode
    opcode->branchTo = newOffset;
}

//handle 'else'
sdword etgElse(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    etgbranch *opcode = (etgbranch *)dest;
    sdword index;

    opcode->opcode = EOP_Goto;
    opcode->codeBlock = etgParseMode;
    opcode->branchTo = MINUS1;
    dbgAssertOrIgnore(etgParseMode >= 0 && etgParseMode < ETG_NumberCodeBlocks);
    index = etgExecStack.etgCodeBlock[etgParseMode].offset;
    etgNestFunctionSet(NULL, etgElseComplete, etgParseMode, index, (ubyte *)etgIfOpcode);//set function to be called later
    return(sizeof(etgbranch));
}

//handle the 'goto'
sdword etgLabelGoto(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    etgvarentry *label;
    etgbranch *opcode = (etgbranch *)dest;

    label = etgVariableFind(params);
    opcode->opcode = EOP_Goto;
    if (label == NULL)
    {                                                       //if label not found
        opcode->opcode |= EOP_Resolve;                      //resolve
        opcode->branchTo = (memsize)memStringDupe(params);   //save name for later
    }
    else
    {
        opcode->branchTo = label->initial;
        opcode->codeBlock = label->codeBlock;
    }
#if ETG_ERROR_CHECKING
    if (label != NULL && label->type != EVT_Label)
    {
        etgLoadErrorf(ETG, "Symbol '%s' not a proper label name.", params);
        return(0);
    }
#endif
    return(sizeof(etgbranch));
}

//handle 'call'
sdword etgLabelCall(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    etgvarentry *label;
    etgbranch *opcode = (etgbranch *)dest;

    label = etgVariableFind(params);
    opcode->opcode = EOP_Call;
    if (label == NULL)
    {                                                       //if label not found
        opcode->opcode |= EOP_Resolve;                      //resolve
        opcode->branchTo = (memsize)memStringDupe(params);   //save name for later
    }
    else
    {
        opcode->branchTo = label->offset;
        opcode->codeBlock = label->size;
    }
#if ETG_ERROR_CHECKING
    if (label != NULL && label->type != EVT_Label)
    {
        etgLoadErrorf(ETG, "Symbol '%s' not a proper label name.", params);
        return(0);
    }
#endif
    return(sizeof(etgbranch));
}

//handle return
sdword etgReturn(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    etgbranch *opcode = (etgbranch *)dest;
    opcode->opcode = EOP_Return;
    opcode->codeBlock = (sdword) MINUS1;
    opcode->branchTo = MINUS1;
    return(sizeof(etgbranch));
}

//handle the 'label' operator
sdword etgNewLocalLabel(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    char *string;
    sdword offset = -1;
    sdword type;

    string = strtok(params, " \t.,][()");
#if ETG_VERBOSE_LEVEL >= 2
    dbgMessagef("Creating label '%s'", string);
#endif
    if (etgParseMode == EPM_Constant)
    {
        offset = stat->constLength;
        type = EVT_ConstLabel;
    }
    else if (etgParseMode == EPM_Variables)
    {
        offset = stat->variableSize;
        type = EVT_VarLabel;
    }
    else
    {
        offset = etgExecStack.etgCodeBlock[etgParseMode].offset;
        type = EVT_Label;
    }
    etgNewVariableCreate(string, stat, type, FALSE, offset, 0);
    return(0);
}

//handle the 'eventStart' label-function with name and optional parameters
sdword etgNewEventParse(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    char *token;
    sdword opcodeLength;
    ubyte opcode[OPCODE_MAX];

//    token = strtok(params, ",");
#if ETG_ERROR_CHECKING
    if (etgParseMode != EPM_Nothing)
    {
        etgLoadErrorf(ETG, "'eventStart' encountered in a context block.");
        return(0);
    }
    if (etgVariableIndex != 0)
    {
        etgLoadErrorf(ETG, "'eventStart': %d variables already defined for this effect", etgVariableIndex);
        return(0);
    }
#endif

    //set variable mode
    etgParseMode = EPM_Variables;
    token = strtok(params, ",");
    while (token != NULL)
    {                                                       //remaining tokens will be parameters
        while (strchr(" \t", *token) != NULL)
        {                                                   //strip leading whitespace
            token++;
        }
        while (strlen(token) > 0 && token[strlen(token) - 1] == ' ')
        {
            token[strlen(token) - 1] = 0;                   //strip trailing whitespace
        }
        opcodeLength = 0;
        if (!etgOpcodeScan(stat, token, opcode, &opcodeLength))
        {                                                   //call scan function to create the variable
#if ETG_ERROR_CHECKING
            etgLoadErrorf(ETG, "Don't know how to handle '%s'", token);
            return(0);
#endif
        }
#if ETG_ERROR_CHECKING
        if (opcodeLength != 0)
        {                                                   //if an opcode was created
            etgLoadErrorf(ETG, "'%s' wasn't a variable definition", token);
            return(0);
        }
#endif
        token = strtok(NULL, ",");
    }
    //restore to nothing mode
    etgParseMode = EPM_Nothing;
    return(0);
}

//handle 'end', 'yield' and 'delete'
sdword etgEndOpcode(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    etgprocesscontrol *opcode = (etgprocesscontrol *)dest;
    opcode->opcode = EOP_End;
    return(sizeof(etgprocesscontrol));
}
sdword etgYeildOpcode(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    etgprocesscontrol *opcode = (etgprocesscontrol *)dest;
    opcode->opcode = EOP_Yeild;
    return(sizeof(etgprocesscontrol));
}
sdword etgDeleteOpcode(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    etgprocesscontrol *opcode = (etgprocesscontrol *)dest;
    opcode->opcode = EOP_Delete;
//    stat->bSelfDeleting = TRUE;
    bitSet(stat->specialOps, ESO_SelfDeleting);
    return(sizeof(etgprocesscontrol));
}

/*-----------------------------------------------------------------------------
    Name        : etgAlternateAdd
    Description : Allocate a new alternate structure and return it's index
    Inputs      : void
    Outputs     : Actual structure filled in later
    Return      : index of new alternate structre
----------------------------------------------------------------------------*/
udword etgAlternateAdd(void)
{
#if ETG_ERROR_CHECKING
    if (etgAlternateStructIndex > ETG_MaxAlternates)
    {
        etgLoadErrorf(ETG, "Exceeded %d alternates.", ETG_MaxAlternates);
        return(0);
    }
#endif
    etgAlternateStructIndex++;
    return(etgAlternateStructIndex - 1);
}

//complete a previously-started alternate block
void etgAlternateComplete(sdword codeBlock, sdword offset, sdword newOffset, ubyte *userData)
{
#if ETG_ERROR_CHECKING
    udword index;

    if (etgDecisionOpcode == NULL)
    {                                                       //if already creating a decision opcode
        etgLoadErrorf(ETG, "Unexpected '}' associated with 'alternate'.");
        return;
    }
    dbgAssertOrIgnore(etgDecisionOpcode->opcode == EOP_Alternate);
    dbgAssertOrIgnore(etgDecisionOpcode->codeBytes == 0xffffffff);
    for (index = 0; index < etgDecisionOpcode->tableLength; index++)
    {
        if (etgDecisionOpcode->offset[index] == 0xffffffff)
        {
            dbgWarningf(ETG, "Unallocated entry(s) in alternate table.");
            break;
        }
    }
#endif                                                      //set length of full alternate statement
    etgDecisionOpcode->codeBytes = newOffset - offset + sizeof(etgreturn);
    //the offset table will later be re-allocated into one big table
    etgTotalOffsets += etgDecisionOpcode->tableLength;
    etgAlternateTable[etgDecisionOpcode->criteria].alt.type = EAT_Int;
    etgAlternateTable[etgDecisionOpcode->criteria].alt.low = 0;
    etgAlternateTable[etgDecisionOpcode->criteria].alt.high = etgDecisionOpcode->tableLength;
    //now create a dummy return opcode to prevent stack overflow (remember how we pushed earlier?)
    ((etgreturn *)(etgExecStack.etgCodeBlock[etgParseMode].code + etgExecStack.etgCodeBlock[etgParseMode].offset))->opcode = EOP_Return;
    etgExecStack.etgCodeBlock[etgParseMode].offset += sizeof(EOP_Return);

    //now restore previous level of alternate's information
    etgDecisionOpcode = (etgdecision *)userData;
}

//create an alternate opcode
sdword etgAternateStart(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    etgdecision *opcode = (etgdecision *)dest, *oldOpcode;

#if ETG_ERROR_CHECKING
    if (etgDecisionOpcode != NULL)
    {                                                       //if already creating a decision opcode
//        etgLoadErrorf(ETG, "Cannot nest 'alternate' or 'case' instructions.");
        dbgMessagef("Nesting alternate.");
    }
    if (etgParseMode < EPM_Startup || etgParseMode >= ETG_NumberCodeBlocks)
    {
        etgLoadErrorf(ETG, "'alternate' outside of valid code block: %d", etgParseMode);
        return(0);
    }
#endif
    oldOpcode = etgDecisionOpcode;
    opcode->opcode = EOP_Alternate;
    opcode->tableLength = 0;                                //no entries created yet
    opcode->criteria = etgAlternateAdd();                   //add a new alternate criteria
    opcode->codeBytes = 0xffffffff;                         //no code length yet
    opcode->offset = memAlloc(sizeof(decisionoffset) * ETG_DecisionMax, "Decision offset table", 0);

    etgDecisionOpcode = (etgdecision *)(etgExecStack.etgCodeBlock[etgParseMode].code +
                         etgExecStack.etgCodeBlock[etgParseMode].offset);//this is where this opcode will be stored
    etgNestFunctionSet(NULL, etgAlternateComplete, etgParseMode,  //set function to be called later
        etgExecStack.etgCodeBlock[etgParseMode].offset, (ubyte *)oldOpcode);
    etgAlternateTable[opcode->criteria].decision = etgDecisionOpcode;//save opcode reference for later

    return(sizeof(etgdecision));
}

//add a new alternate into the current alternate list
sdword etgAternateSet(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    sdword length, nEntries, index, nScanned;
    udword offset;
    etgreturn *branchOpcode = (etgreturn *)dest;

    nScanned = sscanf(params, "%d", &nEntries);             //scan size of this chunk of alternate table
#if ETG_ERROR_CHECKING
    if (etgDecisionOpcode == NULL)
    {
        etgLoadErrorf(ETG, "'alt' encountered outside an alternate block.");
        return(0);
    }
    if (nScanned != 1)
    {
        etgLoadErrorf(ETG, "Cannot scan '%s' for alternate entry length.", params);
        return(0);
    }
    if (nEntries < 1 || nEntries >= ETG_DecisionMax)
    {
        etgLoadErrorf(ETG, "Invalid alternate length: %d", nEntries);
        return(0);
    }
    dbgAssertOrIgnore(etgDecisionOpcode->opcode == EOP_Alternate);
    if (etgDecisionOpcode->tableLength + nEntries >= ETG_DecisionMax)
    {
        etgLoadErrorf(ETG, "%d alternates exceeds max of %d",
                  etgDecisionOpcode->tableLength + nEntries + 1, ETG_DecisionMax);
        return(0);
    }
#endif
    if (etgDecisionOpcode->tableLength != 0)
    {                                                       //if not first entry in the list
        branchOpcode->opcode = EOP_Return;                  //generate a return opcode
        length = sizeof(etgreturn);
    }
    else
    {
        length = 0;
    }
    offset = (ubyte *)etgDecisionOpcode - etgExecStack.etgCodeBlock[etgParseMode].code;
    for (index = 0; index < nEntries; index++)
    {                                                       //for each alternate entry
        etgDecisionOpcode->offset[etgDecisionOpcode->tableLength] =
            etgExecStack.etgCodeBlock[etgParseMode].offset - offset + length;//store the offset to current instruction
        etgDecisionOpcode->tableLength++;                   //add another entry to the list
    }
    return(length);
}

//handle the closing brace on a 'timeblock'
void etgTimeBlockComplete(sdword codeBlock, sdword offset, sdword newOffset, ubyte *userData)
{
#if ETG_ERROR_CHECKING
    dbgAssertOrIgnore(etgTimeIndexVar != NULL);
#endif
    etgTimeIndexVar = NULL;
}

//handle opening time index block ('timeblock')
sdword etgTimeBlockOpen(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
#if ETG_ERROR_CHECKING
    if (etgTimeIndexVar != NULL)
    {
        etgLoadErrorf(ETG, "Cannot nest 'timeblock's.");
        return(0);
    }
#endif
    etgTimeIndexVar = etgVariableFind(params);
#if ETG_ERROR_CHECKING
    if (etgParseMode != EPM_TimeIndex)
    {
        etgLoadErrorf(ETG, "'timeblock' can only appear in the 'timeindex' block.");
        return(0);
    }
    if (etgTimeIndexVar == NULL)
    {
        etgLoadErrorf(ETG, "'%s' is not a valid variable name.", params);
        return(0);
    }
    if (etgTimeIndexVar->type != EVT_Float && etgTimeIndexVar->type != EVT_Int &&
        etgTimeIndexVar->type != EVT_RGB && etgTimeIndexVar->type != EVT_RGBA)
    {
        etgLoadErrorf(ETG, "'timeblock' variable '%s' is of type %d.",
                  etgTimeIndexVar->name, etgTimeIndexVar->type);
        return(0);
    }
#endif
    if (etgTimeIndexVar->rateOffset == 0xffffffff)
    {                                                       //if this variable has no rate offset associated with it
        etgTimeIndexVar->rateOffset = stat->rateSize;
        switch (etgTimeIndexVar->type)
        {
            case EVT_Int:
            case EVT_Float:
                stat->rateSize += sizeof(udword);
                break;
            case EVT_RGB:
                stat->rateSize += sizeof(etgratergb);
                break;
            case EVT_RGBA:
                stat->rateSize += sizeof(etgratergba);
                break;
            default:
                dbgAssertOrIgnore(FALSE);
        }
    }
    etgNestFunctionSet(NULL, etgTimeBlockComplete, etgParseMode,  //set function to be called later
        etgExecStack.etgCodeBlock[etgParseMode].offset, NULL);
    etgTimeIndexTime = 0.0f;
    return(0);
}

//handle time index entries ('time')
//this is the only way to define one-point effectors
//generates two opcodes: a between and a time-index effector
sdword etgTimeIndexDefine(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    real32 time, valueReal;
    udword valueInt, nScanned;
    color valueRGBA;
    etgopcode opcode;
    etgbetween *betweenOpcode;
    etgseffector *effector;
    etgvarentry *endVar;
    char *param;

#if ETG_ERROR_CHECKING
    if (etgTimeIndexVar == NULL)
    {
        etgLoadErrorf(ETG, "'time' can only appear in a 'timeblock'.");
        return(0);
    }
    if (strchr(params, ',') == NULL)
    {
        etgLoadErrorf(ETG, "'time' operator requires a time and a value parameter. '%s' just won't do.", params);
        return(0);
    }
#endif
    //check the time
    nScanned = sscanf(params, "%f", &time);
#if ETG_ERROR_CHECKING
    if (nScanned != 1)
    {
        etgLoadErrorf(ETG, "Error scanning time from '%s''", params);
        return(0);
    }
    // CAT: no upper bound (HW1's debug check capped the time at 200 s; the exe has no such check, and Cataclysm's
    //      slipgateOpening.ebg uses time(86400, 86400) as a "never" key)
    if (time < 0.0f)
    {
        etgLoadErrorf(ETG, "Invalid time-index in '%s'", params);
        return(0);
    }
    if (time <= etgTimeIndexTime)
    {
        etgLoadErrorf(ETG, "Non-positive time delta from last time-index (%f <= %f).", time, etgTimeIndexTime);
        return(0);
    }
    if (etgTimeIndexIndex >= UDWORD_Max)
    {
        etgLoadErrorf(ETG, "Cannot have more than 255 time index effectors in a single 'timeindex' block.");
        return(0);
    }
#endif
    param = strchr(params, ',');
    while (strchr("\t ,", *param) != NULL)
    {
        param++;
    }
    //read the end-point value, whatever it's type may be
    if ((endVar = etgVariableFind(param)) != NULL)
    {                                                       //if var-current
#if ETG_ERROR_CHECKING
        if (etgTimeIndexVar->type != endVar->type)
        {
            etgLoadErrorf(ETG, "Variables '%s' and '%s' incompatable types for effectors.", etgTimeIndexVar->name, endVar->name);
            return(0);
        }
#endif
        valueInt = endVar->offset;                          //effector end parameter is now offset for variable
        switch (etgTimeIndexVar->type)
        {
            case EVT_Int:
                opcode = EOP_EffectorVI;
                break;
            case EVT_Float:
                opcode = EOP_EffectorVF;
                break;
            case EVT_RGB:
                opcode = EOP_EffectorVCo;
                break;
            case EVT_RGBA:
                opcode = EOP_EffectorVCa;
                break;
            default:
                dbgAssertOrIgnore(FALSE);
        }
    }
    else
    {                                                       //else const->current
        switch (etgTimeIndexVar->type)
        {
            case EVT_Int:
                nScanned = sscanf(param, "%d", &valueInt);
                opcode = EOP_EffectorCI;
                break;
            case EVT_Float:
                nScanned = sscanf(param, "%f", &valueReal);
                valueInt = Real32ToUdword(valueReal);
                opcode = EOP_EffectorCF;
                break;
            case EVT_RGB:
                valueRGBA = etgColorScan(param);
                valueInt = (udword)valueRGBA;
                opcode = EOP_EffectorCCo;
                break;
            case EVT_RGBA:
                valueRGBA = etgColorScan(param);
                valueInt = (udword)valueRGBA;
                opcode = EOP_EffectorCCa;
                break;
            default:
                dbgAssertOrIgnore(FALSE);
        }
#if ETG_ERROR_CHECKING
        if (nScanned != 1)
        {
            etgLoadErrorf(ETG, "Error value from '%s''", param);
            return(0);
        }
#endif
    }
    //create the between opcode
    betweenOpcode = (etgbetween *)dest;
    betweenOpcode->opcode = EOP_Between;
    betweenOpcode->start = etgTimeIndexTime;
    betweenOpcode->end = time;
    betweenOpcode->startVar = betweenOpcode->endVar = 0xffffffff;
    betweenOpcode->codeBytes = sizeof(etgbetween) + sizeof(etgseffector);
    //create the effector opcode
    effector = (etgseffector *)(dest + sizeof(etgbetween));
    effector->opcode = opcode;
    effector->end = valueInt;
    effector->effector = etgTimeIndexVar->offset;
    effector->rate = etgTimeIndexVar->rateOffset;
    effector->effectorID = (ubyte)etgTimeIndexIndex;
    etgTimeIndexIndex++;

    etgTimeIndexTime = time;                                //update the current timeblock time
    return(sizeof(etgbetween) + sizeof(etgseffector));      //size of 2 opcodes
}


//create an 'at' block
void etgAtComplete(sdword codeBlock, sdword offset, sdword newOffset, ubyte *userData)
{
    etgbeforeafterat *opcode;
    //get opcode pointer
    opcode = (etgbeforeafterat *)(etgExecStack.etgCodeBlock[codeBlock].code + offset);
    dbgAssertOrIgnore(opcode->codeBytes == 0xffffffff);
    dbgAssertOrIgnore(codeBlock == etgParseMode);
    opcode->codeBytes = newOffset - offset;
}

//complete the 'between' block
void etgBetweenComplete(sdword codeBlock, sdword offset, sdword newOffset, ubyte *userData)
{
    etgbetween *opcode;
    //get opcode pointer
    opcode = (etgbetween *)(etgExecStack.etgCodeBlock[codeBlock].code + offset);
    dbgAssertOrIgnore(opcode->codeBytes == 0xffffffff);
    dbgAssertOrIgnore(codeBlock == etgParseMode);
    opcode->codeBytes = newOffset - offset;
}

sdword etgBetweenDefine(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    etgbetween *opcode = (etgbetween *)dest;
    sdword nScanned, index;
    etgvarentry *var;
    real32 time;
    udword varOffset;
    char *param;

    opcode->opcode = EOP_Between;

    param = strtok(params, ETG_TokenDelimiters);

    for (index = 0; index < 2; index++)
    {                                                       //start/end parameters
        time = -1.0f;
        varOffset = 0xffffffff;
        if (etgIsNumber(param))
        {
            nScanned = sscanf(param, "%f", &time);
#if ETG_ERROR_CHECKING
            if (nScanned != 1)
            {
                etgLoadErrorf(ETG, "Error scanning '%s' for time.", param);
                return(0);
            }
            if (time < 0)                                   // CAT: no upper bound (see etgTimeIndexDefine)
            {
                etgLoadErrorf(ETG, "Invalid time '%f' for '%s'.", time, opcodeString);
                return(0);
            }
#endif
        }
        else
        {                                                   //else it's a parameter
            var = etgVariableFind(param);
#if ETG_ERROR_CHECKING
            if (var == NULL)
            {
                etgLoadErrorf(ETG, "Variable '%s' not found.", params);
                return(0);
            }
            if (var->type != EVT_Float)
            {
                etgLoadErrorf(ETG, "Variable '%s' not of type float.  Type %d instead", params, var->type);
                return(0);
            }
#endif
            varOffset = var->offset;
        }
        if (index == 0)
        {                                                   //if defining the start time
            opcode->start = time;
            opcode->startVar = varOffset;
        }
        else
        {                                                   //else defining the end time
            opcode->end = time;
            opcode->endVar = varOffset;
        }
        param = strtok(NULL, ETG_TokenDelimiters);
    }
#if ETG_ERROR_CHECKING
    if (etgParseMode != EPM_EachFrame && etgParseMode != EPM_TimeIndex)
    {
        etgLoadErrorf(ETG, "'%s' must appear in timeIndex or eachFrame blocks", opcodeString);
        return(0);
    }
    if (opcode->start >= opcode->end)
    {
        etgLoadErrorf(ETG, "'%s' - end time must be greater than start time (%.2f >= %.2f", opcode->start, opcode->end);
        return(0);
    }
#endif
    opcode->codeBytes = 0xffffffff;

    etgNestFunctionSet(NULL, etgBetweenComplete, etgParseMode,  //set function to be called later
        etgExecStack.etgCodeBlock[etgParseMode].offset, NULL);
    return(sizeof(etgbetween));
}
//handle 'before', 'every', 'after' and 'at'
sdword etgAtDefine(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    etgbeforeafterat *opcode = (etgbeforeafterat *)dest;
    sdword nScanned;
    etgvarentry *var;
    real32 timeDiff;

    if (etgParseNamesMatch(opcodeString, "at"))
    {
        opcode->opcode = EOP_At;
    }
    else if (etgParseNamesMatch(opcodeString, "before"))
    {
        opcode->opcode = EOP_Before;
    }
    else if (etgParseNamesMatch(opcodeString, "after"))
    {
        opcode->opcode = EOP_After;
    }
    else
    {
        opcode->opcode = EOP_Every;
    }
    if (etgIsNumber(params))
    {
        nScanned = sscanf(params, "%f", &opcode->time);
#if ETG_ERROR_CHECKING
        if (etgParseMode != EPM_EachFrame && etgParseMode != EPM_TimeIndex)
        {
            etgLoadErrorf(ETG, "'%s' must appear in timeIndex or eachFrame blocks", opcodeString);
            return(0);
        }
        if (nScanned != 1)
        {
            etgLoadErrorf(ETG, "Error scanning '%s' for time.", params);
            return(0);
        }
        if (opcode->time < 0)                               // CAT: no upper bound (see etgTimeIndexDefine)
        {
            etgLoadErrorf(ETG, "Invalid time '%f' for '%s'.", opcode->time, opcodeString);
            return(0);
        }
#endif
        if (opcode->opcode == EOP_At)
        {
            timeDiff = (real32)fmod((double)opcode->time, (double)ETG_UpdateRoundOff);
            if (timeDiff > ETG_UpdateRoundOff / 2.0f)
            {
                opcode->time -= timeDiff - ETG_UpdateRoundOff / 2.0f;
            }
            else
            {
                opcode->time += ETG_UpdateRoundOff / 2.0f - timeDiff;
            }
        }
    }
    else
    {
        var = etgVariableFind(params);
#if ETG_ERROR_CHECKING
        if (etgParseMode != EPM_EachFrame && etgParseMode != EPM_TimeIndex)
        {
            etgLoadErrorf(ETG, "'%s' must appear in timeIndex or eachFrame blocks", opcodeString);
            return(0);
        }
        if (var == NULL)
        {
            etgLoadErrorf(ETG, "Variable '%s' not found.", params);
            return(0);
        }
        if (var->type != EVT_Float)
        {
            etgLoadErrorf(ETG, "Variable '%s' not of type float.  Type %d instead", params, var->type);
            return(0);
        }
#endif
        opcode->opcode++;                                   //the corresponding variable version should be 1 greater than the constant version
        ((etgbeforeafteratvar *)opcode)->variable = var->offset;
    }
    opcode->codeBytes = 0xffffffff;
    etgNestFunctionSet(NULL, etgAtComplete, etgParseMode,  //set function to be called later
        etgExecStack.etgCodeBlock[etgParseMode].offset, NULL);
    return(sizeof(etgbeforeafterat));
}
//handle the 'particleBlocks' operator
sdword etgNParticleBlocksSet(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    sdword nScanned;

    nScanned = sscanf(params, "%d", &stat->nParticleBlocks);
#if ETG_ERROR_CHECKING
    if (nScanned != 1)
    {
        etgLoadErrorf(ETG, "Error scanning number of particle blocks from '%s'", params);
        return(0);
    }
#endif
    return(0);
}

//handle the 'maxFrequency' operator
sdword etgNHistorySet(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    sdword nScanned;

    nScanned = sscanf(params, "%d", &stat->nHistoryList);
#if ETG_ERROR_CHECKING
    if (nScanned != 1)
    {
        etgLoadErrorf(ETG, "Error scanning number of particle blocks from '%s'", params);
        return(0);
    }
#endif
    return(0);
}

//handle the 'spawn' operator.  Creates a special function call opcode
sdword etgSpawnParse(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    etgfunctioncall *call = (etgfunctioncall *)dest;
    char *param;
    udword nParams = 2, nScanned;
    etgeffectstatic *spawnEffect;
    etgvarentry *var;

    param = strtok(params, ETG_TokenDelimiters);
#if ETG_ERROR_CHECKING
    if (param == NULL || strlen(param) < 4)
    {
        etgLoadErrorf(ETG, "Syntax error.  Supposed to be: spawn(<name>, <parameter> [, parameters...]");
        return(0);
    }
#endif
    spawnEffect = etgEffectStaticFind(param, TRUE);
    call->parameter[0].type = EVT_Constant;
    call->parameter[0].param = (memsize)spawnEffect;

    call->opcode = EOP_Function;
    call->function = etgSpawnNewEffect;                 // CAT: an argument-array callable
    call->returnValue = MINUS1;
    call->passThis = TRUE;
    while((param = strtok(NULL, ETG_TokenDelimiters)) != NULL)
    {
        if (etgIsNumber(param))
        {                                                   //if it's a number parameter
            call->parameter[nParams].type = EVT_Constant;
            if (strchr(param, '.'))
            {                                               //if it's a floating-point
                nScanned = sscanf(param, "%f", (float*)&call->parameter[nParams].param);
            }
            else
            {                                               //else it's an integer
#ifdef HW_PTR_64
                nScanned = sscanf(param, "%ld", &call->parameter[nParams].param);
#else
                nScanned = sscanf(param, "%d", &call->parameter[nParams].param);
#endif
            }
#if ETG_ERROR_CHECKING
            if (nScanned != 1)
            {
                etgLoadErrorf(ETG, "Error parsing spawn parameter '%s'", param);
                return(0);
            }
#endif
        }
        else
        {                                                   //else it's a varaible parameter
            var = etgVariableFind(param);
#if ETG_ERROR_CHECKING
            if (var == NULL)
            {
                etgLoadErrorf(ETG, "Variable '%s' not found - check your head.", param);
                return(0);
            }
#endif
            switch (var->type)
            {
#if ETG_ERROR_CHECKING
                case EVT_Label:
                    etgLoadErrorf(ETG, "Cannot use label '%s' as a parameter.", param);
                    return(0);
                    break;
#endif
                case EVT_ConstLabel:
                case EVT_VarLabel:
                    call->parameter[nParams].param = var->initial;
                    break;                                  //assigning a constant
                default:
                    call->parameter[nParams].type = (udword)var->type;
                    call->parameter[nParams].param = SCALECAST(var->offset);
            }
            call->parameter[nParams].type = (udword)var->type;
        }
        nParams++;
    }
    call->parameter[1].type = EVT_Constant;                 //second parameter is number of user parameters to pass
    call->parameter[1].param = SCALECAST(nParams - 2);
    call->nParameters = nParams;
    return(etgFunctionSize(nParams));
}

/*-----------------------------------------------------------------------------
    Name        : etgCreationCallback
    Description : particle-creation callback to execute a piece of p-code
    Inputs      : userValue - offset to start execution at.
                  userData - effect we're working on
    Outputs     : executes some p-code, whatever that will do
    Return      :
----------------------------------------------------------------------------*/
void etgCreationCallback(sdword userValue, ubyte *userData)
{
    sdword codeBlock, offset;
    udword opcode;
    sdword size;
    ubyte *pOpcode;
    //TODO(type safety, 2026-04-24): Take an Effect* as argument instead of a ubyte*
    Effect* effect = (Effect*)userData;
    etgeffectstatic* stat = effect->staticinfo;

    //save the code block info for later restoration
    codeBlock = etgExecStack.etgCodeBlockIndex;
    offset = etgExecStack.etgCodeBlock[codeBlock].offset;
    //set the current code offset to the start of the callback code block
    etgExecStack.etgCodeBlock[codeBlock].offset = (udword)userValue;
    //execute this little chunk of code until we find an end code
    while (etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].offset <
           etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].length)
    {
        pOpcode = etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].code + etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].offset;
        opcode = *((udword *)pOpcode);                      //get an opcode
#if ETG_ERROR_CHECKING
        if (opcode < 0 || opcode >= EOP_LastOp)
        {
            dbgFatalf(DBG_Loc, "Effect '%s' has a bad opcode in code segment %d offset %d", stat->name, etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].code, etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].offset);
        }
        if (etgHandleTable[opcode].function == NULL)
        {
            dbgFatalf(DBG_Loc, "etgEffectCodeExecute: NULL opcode %d", opcode);
        }
#endif
        //execute the opcode
        size = etgHandleTable[opcode].function(effect, stat, pOpcode);
        etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].offset += size;
    }
    //restore the code block info
    etgExecStack.etgCodeBlockIndex = codeBlock;
    etgExecStack.etgCodeBlock[codeBlock].offset = offset;
    //p-code back to normal, keep creating particles
}

/*-----------------------------------------------------------------------------
    Name        : etgCreateCallbackSetup
    Description : Called just before a particle creation, sets the particle-
                    creation callback.
    Inputs      : CAT: an ETG callable (0x0046e390): args[0] effect, args[1] offset to p-code to execute
    Outputs     : just sets callback to the correct p-code
    Return      : void
----------------------------------------------------------------------------*/
memsize etgCreateCallbackSetup(etgarg *args)
{
    partCreateCallbackSet(etgCreationCallback, etgArgInt(args, 1), etgArgPtr(args, 0, ubyte *));
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : etgCallbackOpen
    Description : Open the callback code block for particle creation.
    Inputs      : About the same as for block close functions.
    Outputs     : Creates some new opcodes: a call to partCreateCallbackSet and
                    a branch.  Here is an example of a particle creation callback
                    code and corresponding p-code representation:
    createCircles(1, 0)         EOP_Function(partCreateCallbackSet), EOP_Function(createCircles)
    {                       +---branch
        alternate {...}     |       alternate
        ...other code       |       ...other code
    }                       |   end
    ..continue with code    +-->..continue with p-code
    Return      : void
----------------------------------------------------------------------------*/
void etgCallbackOpen(sdword codeBlock, sdword offset, ubyte *userData)
{
    etgfunctioncall *call;                       //we can create a local call because it only has 1 parameter
    etgbranch *branch;                           //branch to skip over the alternate code
#if ETG_ERROR_CHECKING
    etgfunctioncall *createCall;
    //first, let's take move the existing function opcode for the create()
    //function forward enough to fit the call to partCreateCallbackSet
    createCall = (etgfunctioncall *)userData;
    dbgAssertOrIgnore(createCall->opcode == EOP_Function && createCall->nParameters == 2);
#endif
    memmove(userData + etgFunctionSize(1), userData, etgFunctionSize(2));//move the function forward
    //set up the call to partCreateCallbackSet()
    call = (etgfunctioncall *)userData;
    call->opcode = EOP_Function;
    call->function = etgCreateCallbackSetup;                 // CAT: an argument-array callable
    call->passThis = TRUE;
    call->returnValue = MINUS1;
    call->nParameters = 1;
    call->parameter[0].param = (memsize)etgExecStack.etgCodeBlock[etgParseMode].offset +
            etgFunctionSize(1) + sizeof(etgbranch);         //callback should skip these functions and the branch
    call->parameter[0].type = EVT_Constant;
    etgExecStack.etgCodeBlock[etgParseMode].offset += etgFunctionSize(1);//update for size of function call
    //now create the branch opcode
    branch = (etgbranch *)(userData + etgFunctionSize(2) + etgFunctionSize(1));
    dbgAssertOrIgnore(etgNestLevel >= 1);                           //ensure sanity
    etgNestStack[etgNestLevel - 1].userData = (ubyte *)branch;//set new callback user data for when we're closing the code block
    branch->opcode = EOP_Goto;
    branch->codeBlock = codeBlock;
    branch->branchTo = MINUS1;
    etgExecStack.etgCodeBlock[etgParseMode].offset += sizeof(etgbranch);//update for size of branch
}

/*-----------------------------------------------------------------------------
    Name        : etgCallbackClose
    Description : Close a particle creation call-back code block
    Inputs      : standard except for -
                  userData - pointer to the branch opcode at the start of the block
    Outputs     : updates the branch opcode
    Return      :
----------------------------------------------------------------------------*/
void etgCallbackClose(memsize codeBlock, sdword offset, sdword newOffset, ubyte *userData)
{
    etgbranch *branch = (etgbranch *)userData;
    sdword *offsetVar;
    etgprocesscontrol *opcode;

    //create an end opcode at end of block
    dbgAssertOrIgnore(etgParseMode >= 0 && etgParseMode < ETG_NumberCodeBlocks);
    offsetVar = (sdword*)&etgExecStack.etgCodeBlock[etgParseMode].offset;
    opcode = (etgprocesscontrol *)(etgExecStack.etgCodeBlock[etgParseMode].code + *offsetVar);
    opcode->opcode = EOP_End;                               //create the opcode
    *offsetVar += sizeof(etgprocesscontrol);                //and update pointer

    //update the branch opcode as start of block
    dbgAssertOrIgnore(branch->opcode == EOP_Goto || branch->branchTo == 0xffffffff);
    branch->branchTo = newOffset + sizeof(etgprocesscontrol); //fill in the branch opcode
}

//handle a variety of 'createXXX' functions; all but 'createEffects'.  This has
//a special-case parsing function to handle particle-creation call-back code.
void etgCreationResolve(struct etgeffectstatic *stat, etgfunctioncall *call)
{
    etgNestFunctionSet(etgCallbackOpen, (nestfunction)etgCallbackClose, etgParseMode,//set callbacks to structure the callback code
        etgExecStack.etgCodeBlock[etgParseMode].offset,
        (ubyte *)(etgExecStack.etgCodeBlock[etgParseMode].code +
                  etgExecStack.etgCodeBlock[etgParseMode].offset));
}

//handle 'etgDepthWrite'
void etgDepthWriteResolve(struct etgeffectstatic *stat, etgfunctioncall *call)
{
    if (call->parameter[0].type == EVT_Constant && call->parameter[0].param == 0)
    {                                                       //if setting no depth write
        bitSet(stat->specialOps, ESO_SortForward);          //this effect has to sort forward
    }
}

//handle the 'createEffects' operator.  Creates a special function call opcode
//parameters are: this, static, number, variance, nParams, ...
sdword etgCreateEffectsParse(struct etgeffectstatic *stat, ubyte *dest, char *opcodeString, char *params, char *ret)
{
    etgfunctioncall *call = (etgfunctioncall *)dest;
    char *param;
    udword nParams = 4, nScanned, index, value;
    etgeffectstatic *spawnEffect;
    etgvarentry *var;

    param = strtok(params, ETG_TokenDelimiters);
#if ETG_ERROR_CHECKING
    if (param == NULL || strlen(param) < 4)
    {
        etgLoadErrorf(ETG, "Syntax error.  Supposed to be: spawn(<name>, <parameter> [, parameters...]");
        return(0);
    }
#endif
    spawnEffect = etgEffectStaticFind(param, TRUE);
    call->parameter[0].type = EVT_Constant;
    call->parameter[0].param = (memsize)spawnEffect;

    //scan in number/distribution of effect-particles
    for (index = 1; index <= 2; index++)
    {
        param = strtok(NULL, ETG_TokenDelimiters);
#if ETG_ERROR_CHECKING
        if (param == NULL)
        {
            etgLoadErrorf(ETG, "Syntax error.  Supposed to be: %s(<name>, <parameter> [, parameters...]", opcodeString);
            return(0);
        }
#endif
        if (etgIsNumber(param))
        {
            nScanned = sscanf(param, "%d", &value);
#if ETG_ERROR_CHECKING
            if (nScanned != 1 || value < 0 || value > 1000)
            {
                etgLoadErrorf(ETG, "Error scanning '%s' for number of effects/distribution.", param);
                return(0);
            }
#endif
            call->parameter[index].type = EVT_Constant;
            call->parameter[index].param = value;
        }
        else
        {
            var = etgVariableFind(param);
#if ETG_ERROR_CHECKING
            if (var == NULL)
            {
                etgLoadErrorf(ETG, "Cannot find variable '%s'", param);
                return(0);
            }
            if (var->type != EVT_Int)
            {
                etgLoadErrorf(ETG, "Variable '%s' not of type integer.", param);
                return(0);
            }
#endif
            call->parameter[index].type = EVT_Int;
            call->parameter[index].param = var->offset;
        }
    }
    call->opcode = EOP_Function;
    call->function = etgCreateEffects;                 // CAT: an argument-array callable
    call->returnValue = MINUS1;
    call->passThis = TRUE;
    while((param = strtok(NULL, ETG_TokenDelimiters)) != NULL)
    {
        if (etgIsNumber(param))
        {                                                   //if it's a number parameter
            call->parameter[nParams].type = EVT_Constant;
            if (strchr(param, '.'))
            {                                               //if it's a floating-point
                nScanned = sscanf(param, "%f", (float*)&call->parameter[nParams].param);
            }
            else
            {                                               //else it's an integer
#ifdef HW_PTR_64
                nScanned = sscanf(param, "%ld", &call->parameter[nParams].param);
#else
                nScanned = sscanf(param, "%d", &call->parameter[nParams].param);
#endif
            }
#if ETG_ERROR_CHECKING
            if (nScanned != 1)
            {
                etgLoadErrorf(ETG, "Error parsing spawn parameter '%s'", param);
                return(0);
            }
#endif
        }
        else
        {                                                   //else it's a varaible parameter
            var = etgVariableFind(param);
#if ETG_ERROR_CHECKING
            if (var == NULL)
            {
                etgLoadErrorf(ETG, "Variable '%s' not found - check your head.", param);
                return(0);
            }
#endif
            switch (var->type)
            {
#if ETG_ERROR_CHECKING
                case EVT_Label:
                    etgLoadErrorf(ETG, "Cannot use label '%s' as a parameter.", param);
                return(0);
                    break;
#endif
                case EVT_ConstLabel:
                case EVT_VarLabel:
                    call->parameter[nParams].param = var->initial;
                    break;                                  //assigning a constant
                default:
                    call->parameter[nParams].type = (udword)var->type;
                    call->parameter[nParams].param = var->offset;
            }
            call->parameter[nParams].type = (udword)var->type;
        }
        nParams++;
    }
    call->parameter[3].type = EVT_Constant;                 //4th parameter is number of user parameters to pass
    call->parameter[3].param = SCALECAST(nParams - 4);
    call->nParameters = nParams;
    return(etgFunctionSize(nParams));
}

/*-----------------------------------------------------------------------------
    Effect opcode processing:
-----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        :
    Description : Effect-opcode processing functions have this general format:
    Inputs      : effect - pointer to the effect we're processing
                  effectStatic - pointer to effect static info (needed?)
                  opcode - pointer to opcode structure
    Outputs     : Processes opcode in an opcode-specific way
    Return      : Size of opcode structure (amount to update code pointer)
----------------------------------------------------------------------------*/
sdword etgNOP(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    //nop's don't do much
    return(sizeof(etgnop));
}

//handle variable copy
sdword etgVarCopy(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    *((udword *)(effect->variable + ((etgvariablecopy *)opcode)->dest)) =
        *((udword *)(effect->variable + ((etgvariablecopy *)opcode)->source));
    return(sizeof(etgvariablecopy));
}

//handle variable assign
sdword etgVarAssign(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    *((udword *)(effect->variable + ((etgvariablecopy *)opcode)->dest)) = ((etgvariablecopy *)opcode)->source;
    return(sizeof(etgvariablecopy));
}

/*-----------------------------------------------------------------------------
    Name        : etgFunctionCall
    Description : handle function calls
    Inputs      : effect, stat, opcode - standard opcode handler parameters
    Outputs     : CAT: builds the argument array the exe builds (etgFunctionCall 0x0046e7b0): the effect first when
                  passThis is set, then every parameter resolved by type (constant and label as given, ConstLabel
                  and VarLabel as pointers into the constant/variable blocks, a variable by its value), and calls
                  the function with that one pointer. HW1 pushed the same values on the stack (x86 only); GoK
                  called a generated wrapper per function.
    Return      : size of the opcode
    Note        : the exe's array holds 10 slots; here it holds 1 + ETG_FunctionCallParams so the longest
                  createEffects/spawn call fits (a call longer than the exe's array overruns its stack there).
----------------------------------------------------------------------------*/
#define ETG_FunctionCallParams      (4 + ETG_NumberParameters)  //createEffects: stat, number, dist, nParams + user params
sdword etgFunctionCall(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    etgfunctioncall *opptr = (etgfunctioncall *)opcode;
    etgarg args[1 + ETG_FunctionCallParams];
    etgarg *arg = args;
    memsize param;
    udword index, nParams = opptr->nParameters;
    memsize returnValue = opptr->returnValue;

    dbgAssertOrIgnore(nParams <= ETG_FunctionCallParams);
    if (opptr->passThis)
    {                                                       //pass a 'this' pointer first
        *arg++ = (etgarg)effect;
    }
    for (index = 0; index < nParams; index++)
    {                                                       //for each parameter
        param = opptr->parameter[index].param;
        switch (opptr->parameter[index].type)
        {
            case EVT_Constant:
            case EVT_Label:
                break;
            case EVT_ConstLabel:
                param = (memsize)stat->constData + param;
                break;
            case EVT_VarLabel:
                param = (memsize)effect->variable + param;
                break;
            default:
                param = *((memsize *)(effect->variable + param));
                break;
        }
        *arg++ = (etgarg)param;
    }
    param = opptr->function(args);                          //call the function
    if (returnValue != MINUS1)                              //if a return value is desired
    {
        *((memsize *)(effect->variable + returnValue)) = param;//set the return parameter
    }
    return(etgFunctionSize(nParams));
}

//handle 'end'
sdword etgEnd(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    return(0x100000);                                       //!!!this should put a stop to the execution
}

//handle 'delete'
sdword etgDelete(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    etgExecStack.etgDeleteFlag = TRUE;                      //make it delete itself
    return(0x100000);                                       //!!!this should put a stop to the execution
}

//handle 'if'
sdword etgCompareVCI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword var;

    var = *((udword *)(effect->variable + ((etgconditional *)opcode)->param0));
    if (var == ((etgconditional *)opcode)->param1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgNotEqualVCI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword var;

    var = *((udword *)(effect->variable + ((etgconditional *)opcode)->param0));
    if (var != ((etgconditional *)opcode)->param1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgGreaterVCI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword var;

    var = *((udword *)(effect->variable + ((etgconditional *)opcode)->param0));
    if (var > ((etgconditional *)opcode)->param1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgGreaterEqualVCI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword var;

    var = *((udword *)(effect->variable + ((etgconditional *)opcode)->param0));
    if (var >= ((etgconditional *)opcode)->param1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgLessVCI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword var;

    var = *((udword *)(effect->variable + ((etgconditional *)opcode)->param0));
    if (var < ((etgconditional *)opcode)->param1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgLessEqualVCI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword var;

    var = *((udword *)(effect->variable + ((etgconditional *)opcode)->param0));
    if (var <= ((etgconditional *)opcode)->param1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

//handle 'if'
sdword etgCompareVCF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    float var;

    var = *((float *)(effect->variable + ((etgconditional *)opcode)->param0));
    if (var == UdwordToReal32(((etgconditional *)opcode)->param1))
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgNotEqualVCF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    float var;

    var = *((float *)(effect->variable + ((etgconditional *)opcode)->param0));
    if (var != UdwordToReal32(((etgconditional *)opcode)->param1))
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgGreaterVCF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    float var;

    var = *((float *)(effect->variable + ((etgconditional *)opcode)->param0));
    if (var > UdwordToReal32(((etgconditional *)opcode)->param1))
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgGreaterEqualVCF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    float var;

    var = *((float *)(effect->variable + ((etgconditional *)opcode)->param0));
    if (var >= UdwordToReal32(((etgconditional *)opcode)->param1))
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgLessVCF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    float var;

    var = *((float *)(effect->variable + ((etgconditional *)opcode)->param0));
    if (var < UdwordToReal32(((etgconditional *)opcode)->param1))
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgLessEqualVCF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    float var;

    var = *((float *)(effect->variable + ((etgconditional *)opcode)->param0));
    if (var <= UdwordToReal32(((etgconditional *)opcode)->param1))
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgEqualVVI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword var0, var1;

    var0 = *((udword *)(effect->variable + ((etgconditional *)opcode)->param0));
    var1 = *((udword *)(effect->variable + ((etgconditional *)opcode)->param1));
    if (var0 == var1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgNotEqualVVI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword var0, var1;

    var0 = *((udword *)(effect->variable + ((etgconditional *)opcode)->param0));
    var1 = *((udword *)(effect->variable + ((etgconditional *)opcode)->param1));
    if (var0 != var1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgGreaterVVI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword var0, var1;

    var0 = *((udword *)(effect->variable + ((etgconditional *)opcode)->param0));
    var1 = *((udword *)(effect->variable + ((etgconditional *)opcode)->param1));
    if (var0 > var1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgGreaterEqualVVI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword var0, var1;

    var0 = *((udword *)(effect->variable + ((etgconditional *)opcode)->param0));
    var1 = *((udword *)(effect->variable + ((etgconditional *)opcode)->param1));
    if (var0 >= var1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgLessVVI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword var0, var1;

    var0 = *((udword *)(effect->variable + ((etgconditional *)opcode)->param0));
    var1 = *((udword *)(effect->variable + ((etgconditional *)opcode)->param1));
    if (var0 < var1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgLessEqualVVI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword var0, var1;

    var0 = *((udword *)(effect->variable + ((etgconditional *)opcode)->param0));
    var1 = *((udword *)(effect->variable + ((etgconditional *)opcode)->param1));
    if (var0 <= var1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgEqualVVF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    real32 var0, var1;

    var0 = *((real32 *)(effect->variable + ((etgconditional *)opcode)->param0));
    var1 = *((real32 *)(effect->variable + ((etgconditional *)opcode)->param1));
    if (var0 == var1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgNotEqualVVF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    real32 var0, var1;

    var0 = *((real32 *)(effect->variable + ((etgconditional *)opcode)->param0));
    var1 = *((real32 *)(effect->variable + ((etgconditional *)opcode)->param1));
    if (var0 != var1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgGreaterVVF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    real32 var0, var1;

    var0 = *((real32 *)(effect->variable + ((etgconditional *)opcode)->param0));
    var1 = *((real32 *)(effect->variable + ((etgconditional *)opcode)->param1));
    if (var0 > var1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgGreaterEqualVVF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    real32 var0, var1;

    var0 = *((real32 *)(effect->variable + ((etgconditional *)opcode)->param0));
    var1 = *((real32 *)(effect->variable + ((etgconditional *)opcode)->param1));
    if (var0 >= var1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgLessVVF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    real32 var0, var1;

    var0 = *((real32 *)(effect->variable + ((etgconditional *)opcode)->param0));
    var1 = *((real32 *)(effect->variable + ((etgconditional *)opcode)->param1));
    if (var0 < var1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

sdword etgLessEqualVVF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    real32 var0, var1;

    var0 = *((real32 *)(effect->variable + ((etgconditional *)opcode)->param0));
    var1 = *((real32 *)(effect->variable + ((etgconditional *)opcode)->param1));
    if (var0 <= var1)
    {                                                       //if compare succeeds
        return(sizeof(etgconditional));                     //just size of this opcode
    }                                                       //else condition failed
    return(((etgconditional *)opcode)->codeBytes);
}

//handle 'goto' or automatically-generated branches
sdword etgBranchTo(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    etgbranch *branchTo = (etgbranch *)opcode;
    etgExecStack.etgCodeBlockIndex = branchTo->codeBlock;
    etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].offset = branchTo->branchTo;
    return(0);
}

//handle 'call' or automatically-generated branches
#define ETG_CallStackSize       20
sdword etgCallStackIndex = 0;
etgcallstackentry etgCallStack[ETG_CallStackSize];
sdword etgCallTo(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    etgbranch *branchTo = (etgbranch *)opcode;
#if ETG_ERROR_CHECKING
    if (etgCallStackIndex >= ETG_CallStackSize)
    {
        dbgFatalf(DBG_Loc, "Overflowed ETG call stack of depth %d.", ETG_CallStackSize);
    }
#endif
    etgCallStack[etgCallStackIndex].codeBlock = etgExecStack.etgCodeBlockIndex;
    etgCallStack[etgCallStackIndex].codeOffset = etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].offset + sizeof(etgbranch);
    etgCallStackIndex++;
    etgExecStack.etgCodeBlockIndex = branchTo->codeBlock;
    etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].offset = branchTo->branchTo;
    return(0);
}

//handle 'return' by popping a member off the call stack
sdword etgReturnTo(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
#if ETG_ERROR_CHECKING
    if (etgCallStackIndex < 1)
    {
        dbgFatal(DBG_Loc, "Underflowed ETG call stack.");
    }
#endif
    etgCallStackIndex--;
    etgExecStack.etgCodeBlockIndex = etgCallStack[etgCallStackIndex].codeBlock;
    etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].offset = etgCallStack[etgCallStackIndex].codeOffset;
    return(0);
}

//handle 'alternate' by pushing the address of the end of the alternate block onto
//the call stack and then selecting the proper alternate from the offset table.
//When the alternate code is finished, the return will return it to the end of
//the alternate block, rather than the start
sdword etgAlternate(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword alt;
    etgalternate *decision;

#if ETG_ERROR_CHECKING
    if (etgCallStackIndex >= ETG_CallStackSize)
    {
        dbgFatalf(DBG_Loc, "Overflowed ETG call stack of depth %d.", ETG_CallStackSize);
    }
#endif
    etgCallStack[etgCallStackIndex].codeBlock = etgExecStack.etgCodeBlockIndex;
    etgCallStack[etgCallStackIndex].codeOffset =            //prepare the reture address
        etgExecStack.etgCodeBlock[etgExecStack.etgCodeBlockIndex].offset + ((etgdecision *)opcode)->codeBytes;
    etgCallStackIndex++;                                    //push it onto stack
//    alt = effect->alternate[((etgdecision *)opcode)->criteria];//get alternate number
    decision = &stat->decisions[((etgdecision*)opcode)->criteria];
    alt = ranRandom(RANDOM_ETG) % (decision->high - decision->low) + decision->low;
    dbgAssertOrIgnore(alt < ((etgdecision *)opcode)->tableLength);
    return(((etgdecision *)opcode)->offset[alt]);
}

//handle 'between' operator
real32 etgEffectorDuration;
sdword etgBetween(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    real32 start, end;

    if (((etgbetween *)opcode)->startVar != 0xffffffff)
    {
        start = *((real32 *)(effect->variable + ((etgbetween *)opcode)->startVar));
    }
    else
    {
        start = ((etgbetween *)opcode)->start;
    }
    if (((etgbetween *)opcode)->endVar != 0xffffffff)
    {
        end = *((real32 *)(effect->variable + ((etgbetween *)opcode)->endVar));
    }
    else
    {
        end = ((etgbetween *)opcode)->end;
    }
    dbgAssertOrIgnore(start < end);
    if (etgTotalTimeElapsed >= start && etgTotalTimeElapsed < end)
    {                                                       //if within the time constraints
        etgEffectorDuration = end - start;
        return(sizeof(etgbetween));                         //return size of this structure
    }
    return(((etgbetween *)opcode)->codeBytes);              //else return the amount to skip
}

//handle 'at' operator
sdword etgAt(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    real32 timeDiff;

    timeDiff = etgTotalTimeElapsed - ((etgbeforeafterat *)opcode)->time;

    if (timeDiff >= 0.0000005f && timeDiff < etgTimeElapsed + 0.0000005f)
    {                                                       //if this is the frame
        return(sizeof(etgbeforeafterat));                   //just skip over this opcode
    }
    return(((etgbeforeafterat *)opcode)->codeBytes);        //skip over entire block
}

//handle 'at' operator with a variable for a parameter
sdword etgAtVar(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    real32 timeDiff;
    real32 variable;

    variable = *((real32 *)(effect->variable + ((etgbeforeafteratvar *)opcode)->variable));
    // CAT: no range check (etgAtVar 0x0046eed0 has none; Cataclysm effects set an 'at' variable to 1e8 for "never",
    //      which HW1's debug check rejected)
    timeDiff = etgTotalTimeElapsed - variable;

    if (timeDiff >= 0.0000005f && timeDiff < etgTimeElapsed + 0.0000005f)
    {                                                       //if this is the frame
        return(sizeof(etgbeforeafterat));                   //just skip over this opcode
    }
    return(((etgbeforeafterat *)opcode)->codeBytes);        //skip over entire block
}

//handle the 'before' operator
sdword etgBefore(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    if (((etgbeforeafterat *)opcode)->time > etgTotalTimeElapsed)
    {                                                       //if it is before
        return(sizeof(etgbeforeafterat));                   //just skip over this opcode
    }
    return(((etgbeforeafterat *)opcode)->codeBytes);        //skip over entire block
}
sdword etgBeforeVar(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    real32 variable;

    variable = *((real32 *)(effect->variable + ((etgbeforeafteratvar *)opcode)->variable));
    // CAT: no range check (etgBeforeVar 0x0046ef40, etgAfterVar 0x0046ef90 have none)
    if (variable > etgTotalTimeElapsed)
    {                                                       //if it is before
        return(sizeof(etgbeforeafterat));                   //just skip over this opcode
    }
    return(((etgbeforeafterat *)opcode)->codeBytes);        //skip over entire block
}

//handle the 'after' operator
sdword etgAfter(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    if (((etgbeforeafterat *)opcode)->time <= etgTotalTimeElapsed)
    {                                                       //if it is before
        return(sizeof(etgbeforeafterat));                   //just skip over this opcode
    }
    return(((etgbeforeafterat *)opcode)->codeBytes);        //skip over entire block
}
sdword etgAfterVar(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    real32 variable;

    variable = *((real32 *)(effect->variable + ((etgbeforeafteratvar *)opcode)->variable));
    if (variable <= etgTotalTimeElapsed)
    {                                                       //if it is before
        return(sizeof(etgbeforeafterat));                   //just skip over this opcode
    }
    return(((etgbeforeafterat *)opcode)->codeBytes);        //skip over entire block
}


//handle 'every' operator for constants and variables
#define ETG_FrameScale              65536
sdword etgEvery(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword time, timeElapsed, totalTime;

    time = (udword)(((etgbeforeafterat *)opcode)->time * ETG_FrameScale);
    timeElapsed = (udword)(etgTimeElapsed * ETG_FrameScale);
    totalTime = (udword)(etgTotalTimeElapsed * ETG_FrameScale);

    if (time == 0)
    {                                                       // CAT: a zero period runs the block every frame (etgEvery 0x0046efd6) instead of dividing by zero
        return(sizeof(etgbeforeafterat));
    }
    if (totalTime % time < timeElapsed)
    {                                                       //if this is the frame
        return(sizeof(etgbeforeafterat));                   //just skip over this opcode
    }
    return(((etgbeforeafterat *)opcode)->codeBytes);        //skip over entire block
}
sdword etgEveryVar(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword time, timeElapsed, totalTime;
    real32 variable;

    variable = *((real32 *)(effect->variable + ((etgbeforeafteratvar *)opcode)->variable));
    // CAT: no range check (etgEveryVar 0x0046f020 has none)
    time = (udword)(variable * ETG_FrameScale);
    timeElapsed = (udword)(etgTimeElapsed * ETG_FrameScale);
    totalTime = (udword)(etgTotalTimeElapsed * ETG_FrameScale);

    if (time == 0)
    {                                                       // CAT: a zero period runs the block every frame (etgEveryVar 0x0046f043) instead of dividing by zero
        return(sizeof(etgbeforeafterat));
    }
    if (totalTime % time <= timeElapsed + 1)
    {                                                       //if this is the frame
        return(sizeof(etgbeforeafterat));                   //just skip over this opcode
    }
    return(((etgbeforeafterat *)opcode)->codeBytes);        //skip over entire block
}

//handle EOP_EffectorCI
sdword etgEffectorCI(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword offset = ((etgseffector *)opcode)->effector;
    sdword *rate, *var, end;

    rate = (sdword *)(effect->variableRate + ((etgseffector *)opcode)->rate);
    var = (sdword *)(effect->variable + ((etgseffector *)opcode)->effector);
    if (effect->effectID[offset / 4] != ((etgseffector *)opcode)->effectorID)
    {                                                       //if effector just now starting
        *rate = *var;                                       //in the case of integers, the rate is really the start setting
//        *rate = (end - *var) * (sdword)(1.0f / etgEffectorDuration);//rate to be scaled by time delta
//        *rate = (end - var) * (sdword)
        effect->effectID[offset / 4] = ((etgseffector *)opcode)->effectorID;
    }
//!!!    *var += *rate * etgTimeElapsed;                         //as default, just update the effector variable by a rate
    end = (sdword)(((etgseffector *)opcode)->end);      //get integer endpoint
    *var = (end - *rate) * UNIVERSE_UPDATE_RATE /
        ((sdword)(etgEffectorDuration * (real32)UNIVERSE_UPDATE_RATE)) + *rate;
    return(sizeof(etgseffector));
}

//handle EOP_EffectorCF
sdword etgEffectorCF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword offset = ((etgseffector *)opcode)->effector;
    real32 *rate, *var, end;

    rate = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate);
    var = (real32 *)(effect->variable + ((etgseffector *)opcode)->effector);
    if (effect->effectID[offset / 4] != ((etgseffector *)opcode)->effectorID)
    {                                                       //if effector just now starting
        end = UdwordToReal32(((etgseffector *)opcode)->end);  //get floating-point endpoint
        *rate = (end - *var) / etgEffectorDuration;         //rate to be scaled by time delta
        effect->effectID[offset / 4] = ((etgseffector *)opcode)->effectorID;
    }
    *var += *rate * etgTimeElapsed;                         //as default, just update the effector variable by a rate
    return(sizeof(etgseffector));
}

//handle EOP_EffectorVF
sdword etgEffectorVF(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword offset = ((etgseffector *)opcode)->effector;
    real32 *rate, *var, *end;

    rate = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate);
    var = (real32 *)(effect->variable + ((etgseffector *)opcode)->effector);
    if (effect->effectID[offset / 4] != ((etgseffector *)opcode)->effectorID)
    {                                                       //if effector just now starting
        //end = UdwordToReal32(((etgseffector *)opcode)->end);  //get floating-point endpoint
        end = (real32 *)(effect->variable + ((etgseffector *)opcode)->end);
        *rate = (*end - *var) / etgEffectorDuration;         //rate to be scaled by time delta
        effect->effectID[offset / 4] = ((etgseffector *)opcode)->effectorID;
    }
    *var += *rate * etgTimeElapsed;                         //as default, just update the effector variable by a rate
    return(sizeof(etgseffector));
}

//handle EOP_EffectorCCo
sdword etgEffectorCCo(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword offset = ((etgseffector *)opcode)->effector;
    real32 *rateRed, *rateGreen, *rateBlue, red, green, blue;
    color end, *var;

    rateRed = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate);
    rateGreen = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate + sizeof(real32));
    rateBlue = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate + sizeof(real32) * 2);
    var = (color *)(effect->variable + ((etgseffector *)opcode)->effector);
    red =  colUbyteToReal(colRed(*var));
    green =colUbyteToReal(colGreen(*var));
    blue = colUbyteToReal(colBlue(*var));
    if (effect->effectID[offset / 4] != ((etgseffector *)opcode)->effectorID)
    {                                                       //if effector just now starting
        end = (color)(((etgseffector *)opcode)->end);       //get color endpoint
        *rateRed = (colUbyteToReal(colRed(end)) -     red) / etgEffectorDuration;//rate to be scaled by time delta
        *rateGreen = (colUbyteToReal(colGreen(end)) - green) / etgEffectorDuration;//rate to be scaled by time delta
        *rateBlue = (colUbyteToReal(colBlue(end)) -   blue) / etgEffectorDuration;//rate to be scaled by time delta
        effect->effectID[offset / 4] = ((etgseffector *)opcode)->effectorID;
    }
//    *var += *rate * etgTimeElapsed;                         //as default, just update the effector variable by a rate
    red =   red + *rateRed * etgTimeElapsed;                 //update the values per-channel
    green = red + *rateGreen * etgTimeElapsed;
    blue =  red + *rateBlue * etgTimeElapsed;
    if (red < 0.0f)                                         //clamp the values
    {
        red = 0.0f;
    }
    if (red > 1.0f)
    {
        red = 1.0f;
    }
    if (green < 0.0f)
    {
        green = 0.0f;
    }
    if (green > 1.0f)
    {
        green = 1.0f;
    }
    if (blue < 0.0f)
    {
        blue = 0.0f;
    }
    if (blue > 1.0f)
    {
        blue = 1.0f;
    }                                                       //convert back to the color value
    *var = colRGB(colRealToUbyte(red), colRealToUbyte(green), colRealToUbyte(blue));
    return(sizeof(etgseffector));
}

//handle EOP_EffectorVCo
sdword etgEffectorVCo(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword offset = ((etgseffector *)opcode)->effector;
    real32 *rateRed, *rateGreen, *rateBlue, red, green, blue;
    color *end, *var;

    rateRed = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate);
    rateGreen = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate + sizeof(real32));
    rateBlue = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate + sizeof(real32) * 2);
    var = (color *)(effect->variable + ((etgseffector *)opcode)->effector);
    red =  colUbyteToReal(colRed(*var));
    green =colUbyteToReal(colGreen(*var));
    blue = colUbyteToReal(colBlue(*var));
    if (effect->effectID[offset / 4] != ((etgseffector *)opcode)->effectorID)
    {                                                       //if effector just now starting
//        end = (color)(((etgseffector *)opcode)->end);       //get color endpoint
        end = (color *)(effect->variable + ((etgseffector *)opcode)->end);
        *rateRed = (colUbyteToReal(colRed(*end)) -     red) / etgEffectorDuration;//rate to be scaled by time delta
        *rateGreen = (colUbyteToReal(colGreen(*end)) - green) / etgEffectorDuration;//rate to be scaled by time delta
        *rateBlue = (colUbyteToReal(colBlue(*end)) -   blue) / etgEffectorDuration;//rate to be scaled by time delta
        effect->effectID[offset / 4] = ((etgseffector *)opcode)->effectorID;
    }
//    *var += *rate * etgTimeElapsed;                         //as default, just update the effector variable by a rate
    red =   red + *rateRed * etgTimeElapsed;                 //update the values per-channel
    green = green + *rateGreen * etgTimeElapsed;
    blue =  blue + *rateBlue * etgTimeElapsed;
    if (red < 0.0f)                                         //clamp the values
    {
        red = 0.0f;
    }
    if (red > 1.0f)
    {
        red = 1.0f;
    }
    if (green < 0.0f)
    {
        green = 0.0f;
    }
    if (green > 1.0f)
    {
        green = 1.0f;
    }
    if (blue < 0.0f)
    {
        blue = 0.0f;
    }
    if (blue > 1.0f)
    {
        blue = 1.0f;
    }                                                       //convert back to the color value
    *var = colRGB(colRealToUbyte(red), colRealToUbyte(green), colRealToUbyte(blue));
    return(sizeof(etgseffector));
}

//handle EOP_EffectorCCa
sdword etgEffectorCCa(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword offset = ((etgseffector *)opcode)->effector;
    real32 *rateRed, *rateGreen, *rateBlue, *rateAlpha, red, green, blue, alpha;
    color end, *var;

    rateRed = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate);
    rateGreen = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate + sizeof(real32));
    rateBlue = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate + sizeof(real32) * 2);
    rateAlpha = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate + sizeof(real32) * 3);
    var = (color *)(effect->variable + ((etgseffector *)opcode)->effector);
    red =  colUbyteToReal(colRed(*var));
    green =colUbyteToReal(colGreen(*var));
    blue = colUbyteToReal(colBlue(*var));
    alpha = colUbyteToReal(colAlpha(*var));
    if (effect->effectID[offset / 4] != ((etgseffector *)opcode)->effectorID)
    {                                                       //if effector just now starting
        end = (color)(((etgseffector *)opcode)->end);       //get color endpoint
        *rateRed = (colUbyteToReal(colRed(end)) -     red) / etgEffectorDuration;//rate to be scaled by time delta
        *rateGreen = (colUbyteToReal(colGreen(end)) - green) / etgEffectorDuration;//rate to be scaled by time delta
        *rateBlue = (colUbyteToReal(colBlue(end)) -   blue) / etgEffectorDuration;//rate to be scaled by time delta
        *rateAlpha = (colUbyteToReal(colAlpha(end)) - alpha) / etgEffectorDuration;//rate to be scaled by time delta
        effect->effectID[offset / 4] = ((etgseffector *)opcode)->effectorID;
    }
//    *var += *rate * etgTimeElapsed;                         //as default, just update the effector variable by a rate
    red =   red + *rateRed * etgTimeElapsed;                 //update the values per-channel
    green = green + *rateGreen * etgTimeElapsed;
    blue =  blue + *rateBlue * etgTimeElapsed;
    alpha =  alpha + *rateAlpha * etgTimeElapsed;
    if (red < 0.0f)                                         //clamp the values
    {
        red = 0.0f;
    }
    if (red > 1.0f)
    {
        red = 1.0f;
    }
    if (green < 0.0f)
    {
        green = 0.0f;
    }
    if (green > 1.0f)
    {
        green = 1.0f;
    }
    if (blue < 0.0f)
    {
        blue = 0.0f;
    }
    if (blue > 1.0f)
    {
        blue = 1.0f;
    }                                                       //convert back to the color value
    if (alpha < 0.0f)
    {
        alpha = 0.0f;
    }
    if (alpha > 1.0f)
    {
        alpha = 1.0f;
    }                                                       //convert back to the color value
    *var = colRGBA(colRealToUbyte(red), colRealToUbyte(green), colRealToUbyte(blue), colRealToUbyte(alpha));
    return(sizeof(etgseffector));
}

//handle EOP_EffectorVCa
sdword etgEffectorVCa(Effect *effect, struct etgeffectstatic *stat, ubyte *opcode)
{
    udword offset = ((etgseffector *)opcode)->effector;
    real32 *rateRed, *rateGreen, *rateBlue, *rateAlpha, red, green, blue, alpha;
    color *end, *var;

    rateRed = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate);
    rateGreen = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate + sizeof(real32));
    rateBlue = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate + sizeof(real32) * 2);
    rateAlpha = (real32 *)(effect->variableRate + ((etgseffector *)opcode)->rate + sizeof(real32) * 3);
    var = (color *)(effect->variable + ((etgseffector *)opcode)->effector);
    red =  colUbyteToReal(colRed(*var));
    green =colUbyteToReal(colGreen(*var));
    blue = colUbyteToReal(colBlue(*var));
    alpha = colUbyteToReal(colAlpha(*var));
    if (effect->effectID[offset / 4] != ((etgseffector *)opcode)->effectorID)
    {                                                       //if effector just now starting
//        end = (color)(((etgseffector *)opcode)->end);       //get color endpoint
        end = (color *)(effect->variable + ((etgseffector *)opcode)->end);
        *rateRed = (colUbyteToReal(colRed(*end)) -     red) / etgEffectorDuration;//rate to be scaled by time delta
        *rateGreen = (colUbyteToReal(colGreen(*end)) - green) / etgEffectorDuration;//rate to be scaled by time delta
        *rateBlue = (colUbyteToReal(colBlue(*end)) -   blue) / etgEffectorDuration;//rate to be scaled by time delta
        *rateAlpha = (colUbyteToReal(colAlpha(*end)) - alpha) / etgEffectorDuration;//rate to be scaled by time delta
        effect->effectID[offset / 4] = ((etgseffector *)opcode)->effectorID;
    }
//    *var += *rate * etgTimeElapsed;                         //as default, just update the effector variable by a rate
    red =   red + *rateRed * etgTimeElapsed;                 //update the values per-channel
    green = green + *rateGreen * etgTimeElapsed;
    blue =  blue + *rateBlue * etgTimeElapsed;
    alpha =  alpha + *rateAlpha * etgTimeElapsed;
    if (red < 0.0f)                                         //clamp the values
    {
        red = 0.0f;
    }
    if (red > 1.0f)
    {
        red = 1.0f;
    }
    if (green < 0.0f)
    {
        green = 0.0f;
    }
    if (green > 1.0f)
    {
        green = 1.0f;
    }
    if (blue < 0.0f)
    {
        blue = 0.0f;
    }
    if (blue > 1.0f)
    {
        blue = 1.0f;
    }                                                       //convert back to the color value
    if (alpha < 0.0f)
    {
        alpha = 0.0f;
    }
    if (alpha > 1.0f)
    {
        alpha = 1.0f;
    }                                                       //convert back to the color value
    *var = colRGBA(colRealToUbyte(red), colRealToUbyte(green), colRealToUbyte(blue), colRealToUbyte(alpha));
    return(sizeof(etgseffector));
}

/*-----------------------------------------------------------------------------
    Actual p-code function functions, as opposed to p-code functions.
    CAT: each takes the argument array (ETG.h etgarg). Integer and float results come back in the low 32 bits, as
    the exe returns them in eax (etgInc 0x0046fd70 ... etgCos 0x0046fe90).
-----------------------------------------------------------------------------*/

//handle 'inc'
memsize etgInc(etgarg *args)
{
    return((udword)(etgArgUdword(args, 0) + 1));
}

//handle 'dec'
memsize etgDec(etgarg *args)
{
    return((udword)(etgArgUdword(args, 0) - 1));
}

//handle 'fmult'
memsize etgFmult(etgarg *args)
{
    real32 value;
    value = etgArgFloat(args, 0) * etgArgFloat(args, 1);
    return(Real32ToUdword(value));
}

// CAT: new, cat_etgFdegsin 0x0046fdb0 ('fdegsin'): sine of an angle in degrees (x * 6.283185307 * (1/360), x87 fsin)
memsize cat_etgFdegsin(etgarg *args)
{
    real32 value;
    value = (real32)sin((real64)etgArgFloat(args, 0) * 6.283185307 * (1.0 / 360.0));
    return(Real32ToUdword(value));
}

//handle 'fdiv'
memsize etgFdiv(etgarg *args)
{
    real32 value;
    value = etgArgFloat(args, 0) / etgArgFloat(args, 1);
    return(Real32ToUdword(value));
}

//handle 'fadd'
memsize etgFadd(etgarg *args)
{
    real32 value;
    value = etgArgFloat(args, 0) + etgArgFloat(args, 1);
    return(Real32ToUdword(value));
}

//handle 'fsub'
memsize etgFsub(etgarg *args)
{
    real32 value;
    value = etgArgFloat(args, 0) - etgArgFloat(args, 1);
    return(Real32ToUdword(value));
}

//handle 'add'
memsize etgAdd(etgarg *args)
{
    return((udword)(etgArgInt(args, 0) + etgArgInt(args, 1)));
}

//handle 'sub'
memsize etgSub(etgarg *args)
{
    return((udword)(etgArgInt(args, 0) - etgArgInt(args, 1)));
}

//handle 'mult'
memsize etgMult(etgarg *args)
{
    return((udword)(etgArgInt(args, 0) * etgArgInt(args, 1)));
}

//handle 'div'
memsize etgDiv(etgarg *args)
{
    return((udword)(etgArgInt(args, 0) / etgArgInt(args, 1)));
}

//handle 'sin'
memsize etgSin(etgarg *args)
{
    real32 value;
    value = sin(etgArgFloat(args, 0));
    return(Real32ToUdword(value));
}

//handle 'cos'
memsize etgCos(etgarg *args)
{
    real32 value;
    value = cos(etgArgFloat(args, 0));
    return(Real32ToUdword(value));
}

/*-----------------------------------------------------------------------------
    Name        : etgDetatchThisOwner
    Description : Detaches other effects from the said owner.
    Inputs      : effect - effect the search starts on, this effect will not
                    be checked.
                  owner - ship that is dying and is to be removed from references.
                  nToFind - maximum number of effects we can expect to find
                  directions - what direction to search in spaceobj list, forward and/or back
    Outputs     :
    Return      : number of effects detached
----------------------------------------------------------------------------*/
sdword etgDetatchThisOwner(Effect *effect, Ship *owner, sdword nToFind)
{
    Node *fNode, *bNode;
    Effect *thisEffect;
    sdword nDetached;
#if ETG_DETATCH_STATS
    static int nDetachRequests = 0;
    static int nTotalDetached = 0;
    static int nWalks = 0;

    nDetachRequests += nToFind;
#endif

    dbgAssertOrIgnore(nToFind > 0);
    bNode = effect->effectLink.prev;
    fNode = effect->effectLink.next;

    while (bNode || fNode)
    {
        if (fNode != NULL)
        {
#if ETG_DETATCH_STATS
            nWalks++;
#endif
            thisEffect = listGetStructOfNode(fNode);
            if (thisEffect->header.objtype == OBJ_EffectType && thisEffect->owner == owner)
            {
#if ETG_DETATCH_STATS
                nTotalDetached++;
#endif
                thisEffect->owner = NULL;
                nDetached++;
                if (nDetached >= nToFind)
                {
                    break;
                }
            }
            fNode = fNode->prev;
        }
        if (bNode != NULL)
        {
#if ETG_DETATCH_STATS
            nWalks++;
#endif
            thisEffect = listGetStructOfNode(bNode);
            if (thisEffect->header.objtype == OBJ_EffectType && thisEffect->owner == owner)
            {
#if ETG_DETATCH_STATS
                nTotalDetached++;
#endif
                thisEffect->owner = NULL;
                nDetached++;
                if (nDetached == nToFind)
                {
                    break;
                }
            }
            bNode = bNode->prev;
        }
    }
#if ETG_DETATCH_STATS
    if (nTotalDetached >= 10)
    {
        dbgMessagef("etgDetatchThisOwner: detached %d effects for %d requests (%.2f%%) in %d walks (%.2f/per).",
                    nTotalDetached, nDetachRequests, (real32)nTotalDetached / (real32)nDetachRequests * 100.0f, nWalks, (real32)nWalks / (real32)nTotalDetached);
        nDetachRequests = nTotalDetached = nWalks = 0;
    }
#endif
    return(nDetached);
}

//handle the 'deleteParentShip' command
//extern void univWipeShipOutOfExistence(void *ship);                  // totally deletes ship, frees memory
// CAT: an ETG callable: args[0] is the effect (etgParentShipDelete 0x0046ff30 takes a pointer to its 'this' slot)
memsize etgParentShipDelete(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);

    if (effect->owner != NULL)
    {                                                       //if properly attached
        if (effect->owner->header.objtype == OBJ_ShipType)
        {                                                   //to a ship (could be attached to another effect)
            if (bitTest(effect->effectFlags, EAF_PlayingSpeech))
            {                                                   //if the ship is screaming out in pain
                soundEventShipDied(effect->owner);
                bitClear(effect->effectFlags, EAF_PlayingSpeech);
            }
            univWipeShipOutOfExistence(effect->owner);
        }
        else if (effect->owner->header.objtype == OBJ_DerelictType)
        {
            univWipeDerelictOutOfExistance((Derelict *)effect->owner);
        }
        if (effect->nSpawnedAttachedToOwner > 0)
        {
            etgDetatchThisOwner(effect, effect->owner, effect->nSpawnedAttachedToOwner);
        }
        effect->owner = NULL;
    }
    else
    {
#if ETG_VERBOSE_LEVEL >= 1
        dbgMessagef("Effect '%s' tried to delete ship more than once.", effect->staticinfo->name);
#endif
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : etgShipDied
    Description : Remove references to a ship from any effects that may refer
                    to it.
    Inputs      : deadDuck - the ship that is dying
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void etgShipDied(Ship *deadDuck)
{
    Node *node;
    Effect *effect;

    for (node = universe.effectList.head; node != NULL; node = node->next)
    {
        effect = listGetStructOfNode(node);
        if (effect->owner == deadDuck)
        {
            effect->owner = NULL;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_univRemoveEffectReferencesFromShips
    Description : CAT: new (0x005475e0, in the exe's universe code; only the ETG calls it). Gives every ship in
                  the universe and in the hyperspace ether a chance to drop its references to an effect that is
                  going away: CustShipHeader slot 13, CustShipEffectRemoval (statinfo + 0x13C), when set. The
                  slot bodies are ship code (M2-R3 #4 and M2-R4 #4); every header of this tree leaves it NULL.
    Inputs      : effect - the effect being deleted
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void cat_univRemoveEffectReferencesFromShips(Effect *effect)
{
    Node *node;
    Ship *ship;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (ship->staticinfo->custshipheader.CustShipEffectRemoval != NULL)
        {
            ship->staticinfo->custshipheader.CustShipEffectRemoval(ship, effect);
        }
    }
    for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = node->next)
    {
        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
        if (ship->staticinfo->custshipheader.CustShipEffectRemoval != NULL)
        {
            ship->staticinfo->custshipheader.CustShipEffectRemoval(ship, effect);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : etgDeleteEffectsOwnedBy
    Description : Delete all effects that have the specified ship as their owner.
    Inputs      : owner - ship that may be owner of one or more effects
    Outputs     :
    Return      : Number of effects deleted
    Note        : CAT: the exe (0x0046fff0) inlines etgEffectDelete here, with its new steps (ship effect references,
                  particle attachments); calling etgEffectDelete does the same.
----------------------------------------------------------------------------*/
sdword etgDeleteEffectsOwnedBy(Ship *owner)
{
    Node *node, *nextNode;
    Effect *effect;
    sdword nFreed = 0;

    for (node = universe.effectList.head; node != NULL; node = nextNode)
    {
        nextNode = node->next;
        effect = listGetStructOfNode(node);
        if (effect->owner == owner)
        {
            etgEffectDelete(effect);                        //delete the effect
            univRemoveObjFromRenderList((SpaceObj *)effect);//remove from render list
            listDeleteNode(&effect->header.objlink);               //remove from object list
            nFreed++;
        }
    }
    return(nFreed);
}

//handle the 'damageDone' command (callback)
//extern void dmgStopSingleEffect(Effect* effect)
memsize etgDamageDone(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);

    if (effect->owner != NULL)
    {
        dmgStopSingleEffect(effect);
    }
    else
    {
#if ETG_VERBOSE_LEVEL >= 1
        dbgMessagef("Effect '%s' tried to call a damage callback with no owner.", effect->staticinfo->name);
#endif
    }
    etgExecStack.etgDeleteFlag = TRUE;
    return 0;
}

//handle 'getColorScheme' function
memsize etgOwnerColorSchemeGet(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);

    if (effect->owner != NULL)
    {
        return(effect->owner->colorScheme);
    }
    return(0);
}

//handle the 'getEffectVelocity' function
memsize etgEffectVelocityGet(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);
    vector *velVector;
    real32 velocity;
    udword intVelocity;

    if (effect->owner != NULL)
    {
        velVector = &effect->owner->obj.posinfo.velocity;
    }
    else
    {
        velVector = &effect->obj.posinfo.velocity;
    }
    velocity = vecMagnitudeSquared(*velVector);
    velocity = fsqrt(velocity);
    intVelocity = Real32ToUdword(velocity);
    return(intVelocity);
}

//handle the 'hideParentShip' function
memsize etgParentShipHide(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);

    if (effect->owner != NULL)
    {
        univHideShipFromSpheres(effect->owner);
    }
    else
    {
#if ETG_VERBOSE_LEVEL >= 1
        dbgMessagef("Effect '%s' at 0x%x tried to hide ship which was deleted.", effect->staticinfo->name, effect);
#endif
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : cat_etgParentShipOmitFromRenderList
    Description : CAT: new (0x00470190, 'omitParentShipFromRenderList'). Sets the owner's word at +0x1c to 1; the
                  render list builder leaves such objects out (the field is a Cataclysm addition to the object
                  header; its name and its reader belong to the render list port).
    Inputs      : args[0] - the effect
    Outputs     :
    Return      : 0 (the exe returns the owner pointer or 0 in eax; no script uses the value)
----------------------------------------------------------------------------*/
memsize cat_etgParentShipOmitFromRenderList(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);

    if (effect->owner != NULL)
    {
        effect->owner->header.catOmitFromRenderList = 1;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : etgChatterEventPlay
    Description : Play a speech event on the effect's owner object
    Inputs      : args[0] - what effect to play the event on
                  args[1] - what battle chatter event to play
    Outputs     : set's the ship's EAF_PlayingSpeech bit which can later be
                    used to turn the speech event off when the ship is killed.
    Return      :
----------------------------------------------------------------------------*/
memsize etgChatterEventPlay(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);
    battlechatterevent event = (battlechatterevent)etgArgInt(args, 1);

    if (effect->owner != NULL)
    {                                                       //if this effect has an owner
        dbgAssertOrIgnore(event >= 0 && event < BCE_LastBCE);
        if (battleChatterAttempt(SOUND_EVENT_DEFAULT, event, effect->owner, SOUND_EVENT_DEFAULT))
        {
            bitSet(effect->effectFlags, EAF_PlayingSpeech);     //note that we are playing some speech
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : etgDeathCriesPlay
    Description : Play death cry battle chatter for a dying ship
    Inputs      : args[0] - effect who's ship is blowing up
    Outputs     : will play a variety of different battle chatter events based
                    on ship class and whatnot.
    Return      :
----------------------------------------------------------------------------*/
memsize etgDeathCriesPlay(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);
    Ship *owner = effect->owner;

    if (owner != NULL)
    {
        battleShipDyingWithTimeToScream(owner);
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    CAT: the create* callables (etgCreateSprites 0x00470210 ... cat_etgCreateLensFlares 0x00470480): args[0] is the
    effect, args[1] the number of particles, args[2] the distribution. They return the new particle block.
-----------------------------------------------------------------------------*/
static memsize etgCreateParticleSystem(etgarg *args, particleType type)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);
    sdword number = etgArgInt(args, 1), dist = etgArgInt(args, 2);

#if ETG_VERBOSE_LEVEL >= 2
    dbgMessagef("Creating %d +- %d particles of type %d for 0x%x", number, dist, type, effect);
#endif
#if ETG_ERROR_CHECKING
    if (effect->iParticleBlock >= effect->nParticleBlocks)
    {
        dbgFatalf(DBG_Loc, "etgCreateCircles: exceeded %d particle blocks", effect->iParticleBlock);
    }
#endif
    partSetPosition(&effect->obj.posinfo.position);
    partSetCoordSys(&effect->rotatable.rotinfo.coordsys);
    partSetWorldVel(&effect->obj.posinfo.velocity);
    effect->particleBlock[effect->iParticleBlock] =
        (ubyte *)partCreateSystemWithDelta(type, number, dist);
    effect->iParticleBlock++;
    return((memsize)effect->particleBlock[effect->iParticleBlock - 1]);
}

//handle creation of sprite-particles
memsize etgCreateSprites(etgarg *args)
{
    return(etgCreateParticleSystem(args, PART_BILLBOARD));
}

//handle creation of circle-particles
memsize etgCreateCircles(etgarg *args)
{
    memsize trArgs[5] = {(memsize)TR_Invalid, 0, 0, 0, 0};

    partSetTrHandle(trArgs);                                //CAT: an argument-array callable now
    return(etgCreateParticleSystem(args, PART_BILLBOARD));
}

//handle creation of point-particles
memsize etgCreatePoints(etgarg *args)
{
    return(etgCreateParticleSystem(args, PART_POINTS));
}

//handle creation of mesh-particles
memsize etgCreateMeshes(etgarg *args)
{
    return(etgCreateParticleSystem(args, PART_MESH));
}

//handle creation of line-particles
memsize etgCreateLines(etgarg *args)
{
    return(etgCreateParticleSystem(args, PART_LINES));
}

// CAT: new, cat_etgCreateEBolts 0x00470420 ('createEBolts'): electric-bolt particles
memsize cat_etgCreateEBolts(etgarg *args)
{
    return(etgCreateParticleSystem(args, PART_EBOLTS));
}

// CAT: new, cat_etgCreateLensFlares 0x00470480 ('createLensFlares'): lens-flare particles
memsize cat_etgCreateLensFlares(etgarg *args)
{
    return(etgCreateParticleSystem(args, PART_LENSFLARES));
}

//handle the creation of effect-particles
// CAT: an ETG callable (etgCreateEffects 0x004704e0): args[0] effect, [1] effect static, [2] number, [3] distribution,
//      [4] number of user parameters, [5...] the user parameters (etgCreateEffectsParse builds the call)
memsize etgCreateEffects(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);
    etgeffectstatic *stat = etgArgPtr(args, 1, etgeffectstatic *);
    sdword number = etgArgInt(args, 2);
    sdword dist = etgArgInt(args, 3);
    sdword nParams = etgArgInt(args, 4);
    etgarg *userParams = args + 5;
    psysPtr system;
    particle *part;
    sdword index, size;
    vector up, right, LOF, tempVector;
    Effect *newEffect;

    matGetVectFromMatrixCol1(up, effect->rotatable.rotinfo.coordsys);

#if ETG_VERBOSE_LEVEL >= 2
    dbgMessagef("Creating %d +- %d effect-particles for 0x%x", number, dist, effect);
#endif
    if (dist != 0)
    {
        number = ranRandom(RANDOM_ETG) % (dist * 2) + number - dist;
    }
    partSetPosition(&effect->obj.posinfo.position);
    partSetCoordSys(&effect->rotatable.rotinfo.coordsys);
    partSetWorldVel(&effect->obj.posinfo.velocity);
    system = partCreateSystem(PART_POINTS, number);
    for (index = 0, part = (particle*)(system + partHeaderSize(system));
         index < partNumberof(system); index++, part++)
    {
        if (etgFrequencyExceeded(stat))
        {
            continue;
        }
        size = stat->effectSize;
        newEffect = memAlloc(size, "Ef(Effect)", Pyrophoric);   //allocate the new effect
        newEffect->obj.catMeshState = NULL;                 // CAT: +0x80 cleared, +0x84 = a one-byte block
        newEffect->obj.catEffectBlock = memAlloc(1, "EffectBlock", 0);  //      (etgCreateEffects 0x004704e0)
        newEffect->header.objtype = OBJ_EffectType;
        newEffect->header.flags = SOF_Rotatable;
        newEffect->header.catFlags18 = 0;                  // CAT: cleared as the exe does (0x00470669)
        newEffect->staticinfo = stat;
        ClearNode(newEffect->obj.renderlink);
        newEffect->obj.currentLOD = effect->obj.currentLOD;
        newEffect->obj.cameraDistanceSquared = effect->obj.cameraDistanceSquared;
        newEffect->timeElapsed = -part->waitspan;           //use delay from the particle

        SET_MOVING_IMMOBILE(newEffect->obj.posinfo.isMoving);
        newEffect->obj.posinfo.haventCalculatedDist = TRUE;
        newEffect->drag = ((pointSystem*)system)->drag;
        //compute new velocity vector
        if (part->velR != 0.0f)
        {
            vecMultiplyByScalar(part->rvec, part->velR);    //get velocity in effect space
        }
        part->rvec.z = part->velLOF;
        if (bitTest(((pointSystem *)system)->flags, PART_WORLDSPACE))
        {
            newEffect->obj.posinfo.velocity = part->wVel;
            newEffect->obj.posinfo.position = part->position;
        }
        else
        {
            //!!! this is inconsistent with the velocity vector stuff in particle.c !!!
            matMultiplyMatByVec(&newEffect->obj.posinfo.velocity,   //convert to world space
                                &effect->rotatable.rotinfo.coordsys, &part->rvec);
            matMultiplyMatByVec(&newEffect->obj.posinfo.position,   //position in world coords
                                &effect->rotatable.rotinfo.coordsys, &part->position);
            vecAdd(newEffect->obj.posinfo.position,                 //position not relative to parent effect
                   newEffect->obj.posinfo.position, effect->obj.posinfo.position);
        }
        //velocity vector is new LOF, create a coordinate system for it
        matMultiplyMatByVec(&LOF, &effect->rotatable.rotinfo.coordsys, &part->rvec);
        vecCrossProduct(right, LOF, up);
        vecCrossProduct(tempVector, LOF, right);
        vecNormalize(&LOF);
        vecNormalize(&right);
        vecNormalize(&tempVector);

        matPutVectIntoMatrixCol1(tempVector, newEffect->rotatable.rotinfo.coordsys);
        matPutVectIntoMatrixCol2(right, newEffect->rotatable.rotinfo.coordsys);
        matPutVectIntoMatrixCol3(LOF, newEffect->rotatable.rotinfo.coordsys);

        if (!bitTest(((pointSystem *)system)->flags, PART_WORLDSPACE))
        {
            univUpdateObjRotInfo((SpaceObjRot *)newEffect);
        }

        dbgAssertOrIgnore(nParams >= 0 && nParams <= ETG_NumberParameters);
        etgEffectCodeStart(stat, newEffect, nParams, userParams);//get the code a-runnin'

        if (bitTest(stat->specialOps, ESO_SortForward))
        {                                                   //make it sort forward, if applicable
            bitSet(newEffect->header.flags, SOF_AlwaysSortFront);
        }
        newEffect->owner = NULL;                            // no host for this effect
        newEffect->nSpawnedAttachedToOwner = 0;
        newEffect->magnitudeSquared = effect->magnitudeSquared;
        newEffect->spin = etgEffOffset.spin;
        newEffect->deltaSpin = etgEffOffset.deltaSpin;
        listAddNode(&universe.SpaceObjList,&(newEffect->header.objlink),newEffect);
        listAddNode(&universe.effectList,&(newEffect->effectLink),newEffect);
        cat_univShipMeshStateCreate(&newEffect->obj);       // CAT: etgCreateEffects 0x004704e0 (NULL: not a ship)
        if (bitTest(stat->specialOps, ESO_WorldRender))
        {
            bitSet(newEffect->header.flags, SOF_Hide);
            newEffect->header.catOmitFromRenderList = 0;    // CAT: 0x00470930
            univAddToWorldList((Derelict *)newEffect);
        }
        else
        {
            univAddObjToRenderListIf((SpaceObj *)newEffect,(SpaceObj *)effect);     // add to render list if parent ship is in render list
        }
    }
    memFree(system);
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : etgEffectCreate
    Description : Add an effect to the universe.
    Inputs      : stat - what effect to use
                  owner - parent object of this effect, NULL if none
                  pos - position of the effect, NULL to get from parent
                  vel - velocity of the effect, NULL to get from parent
                  coordsys - coordinate system for effect, NULL to get from parent
                  nLips - initial nLips value
                  flags - attachment flags for the effect
                  nParams - number of parameters for the effect code
                  ... - parameters for the effect code
    Outputs     :
    Return      : Newly allocated and started effect.
----------------------------------------------------------------------------*/
Effect* etgEffectCreate(etgeffectstatic* stat, Ship* owner, vector* pos, vector* vel, matrix* coordsys, real32 nLips,
                        udword flags, sdword nParams, ...) {
    sdword index;
    va_list argList;
    etgarg arg[CAT_ETG_CREATE_PARAMETERS];     // CAT: ten in the exe (etgEffectCreate 0x00470990: sub esp,0x28)
    Effect *newEffect;

    newEffect = memAlloc(stat->effectSize, "GE(GenEffect)", Pyrophoric);//allocate the new effect
    newEffect->obj.catMeshState = NULL;                     // CAT: +0x80 cleared, +0x84 = a one-byte block
    newEffect->obj.catEffectBlock = memAlloc(1, "EffectBlock", 0);  //      (etgEffectCreate 0x00470990)

    newEffect->header.objtype = OBJ_EffectType;                    //type of spaceobj
    // CAT: only the attachment bits are copied, and EAF_OwnerCoordSys becomes its own header bit (etgEffectCreate
    //      0x00470990: flags & 8 -> |9, & 0x20, & 0x10, & 0x40 copied, & 0x01000000 -> | 0x20000000); copying the EAF bit
    //      as HW1 did would now set SOF_Clamped (0x01000000 since the SOF_* renumbering)
    newEffect->header.flags = SOF_Rotatable | (flags & EAF_Full);  //basic flags
    if (bitTest(flags, EAF_OwnerCoordSys))
    {
        bitSet(newEffect->header.flags, SOF_OwnerCoordSys);
    }
    newEffect->header.catFlags18 = 0;                      // CAT: cleared (0x004709ea)
    newEffect->staticinfo = stat;             //save static info
    ClearNode(newEffect->obj.renderlink);                       //no in render list
    newEffect->obj.currentLOD = 0;//owner ? ((Ship *)owner)->obj.currentLOD : 0;
    newEffect->obj.cameraDistanceSquared = owner ? owner->obj.cameraDistanceSquared : REALlyBig;
    newEffect->timeElapsed = 0.0f;                          //effect just now starting
    newEffect->drag = 1.0f;                                 //default is no drag at all

    //position
    if (pos == NULL)
    {
        dbgAssertOrIgnore(owner != NULL);
//        if (!bitTest(flags, SOF_AttachPosition))
        {
            newEffect->obj.posinfo.position = owner->obj.posinfo.position;
        }
    }
    else
    {
        newEffect->obj.posinfo.position = *pos;
    }

    //velocity
    if (vel == NULL)
    {
        dbgAssertOrIgnore(owner != NULL);
//        if (!bitTest(flags, SOF_AttachVelocity))
        {
            newEffect->obj.posinfo.velocity = owner->obj.posinfo.velocity;
        }
    }
    else
    {
        newEffect->obj.posinfo.velocity = *vel;
    }

    //coordinate system
    if (coordsys == NULL)
    {
        dbgAssertOrIgnore(owner != NULL);
//        if (!bitTest(flags, SOF_AttachCoordsys))
        {
            newEffect->rotatable.rotinfo.coordsys = owner->rotatable.rotinfo.coordsys;
        }
    }
    else
    {
        newEffect->rotatable.rotinfo.coordsys = *coordsys;
    }
    newEffect->owner = owner;                       // ((Ship *)owner) that effect is attached to
    newEffect->nSpawnedAttachedToOwner = 0;
    if (nLips == 0.0f)
    {
        nLips = 1.0f;
    }
    newEffect->magnitudeSquared = nLips;                    //set initial nLips value

    //prepare to pass the parameters through
    // CAT: the exe copies the variable arguments into an array and passes its address (0x00470b16-0x00470b3c);
    //      the array is the function's ten-dword local block (sub esp,0x28), filled for nParams > 0 with no other
    //      bound: univDeleteDeadShip's big explosion passes ten (M2-R8 oracle-rung-physics)
    dbgAssertOrIgnore(nParams >= 0 && nParams <= CAT_ETG_CREATE_PARAMETERS);
    va_start(argList, nParams);
    for (index = 0; index < nParams; index++)
    {
        arg[index] = va_arg(argList, memsize);
    }
    va_end(argList);                                        //pass all possible parameters

    etgEffectCodeStart(stat, newEffect, nParams, arg);      //get the code a-runnin'
    if (bitTest(stat->specialOps, ESO_SortForward))
    {                                                       //make it sort forward, if applicable
        bitSet(newEffect->header.flags, SOF_AlwaysSortFront);
    }
    listAddNode(&universe.SpaceObjList,&(newEffect->header.objlink),newEffect);
    listAddNode(&universe.effectList,&(newEffect->effectLink),newEffect);
    cat_univShipMeshStateCreate(&newEffect->obj);           // CAT: etgEffectCreate 0x00470990 (NULL: not a ship)
    if (owner != NULL)
    {
        if (bitTest(stat->specialOps, ESO_WorldRender))
        {
            bitSet(newEffect->header.flags, SOF_Hide);
            newEffect->header.catOmitFromRenderList = 0;    // CAT: 0x00470ba3
            univAddToWorldList((Derelict *)newEffect);
        }
        else
        {
            univAddObjToRenderListIf((SpaceObj *)newEffect,(SpaceObj *)owner);      // add to render list if parent ((Ship *)owner) is in render list
        }
    }
    else
    {
        if (bitTest(stat->specialOps, ESO_WorldRender))
        {
            bitSet(newEffect->header.flags, SOF_Hide);
            newEffect->header.catOmitFromRenderList = 0;    // CAT: 0x00470ba3
            univAddToWorldList((Derelict *)newEffect);
        }
        else
        {
            univAddObjToRenderList((SpaceObj *)newEffect);      // add to render list if parent ((Ship *)owner) is in render list
        }
    }
    if (bitTest(stat->specialOps, ESO_SortForward))
    {                                                       //make it sort forward, if applicable
        bitSet(newEffect->header.flags, SOF_AlwaysSortFront);
    }

    return newEffect;
}

/*-----------------------------------------------------------------------------
    Name        : etgHistoryRegisterFunction
    Description : Register an effect creation in this effect's history list
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void etgHistoryRegisterFunction(etgeffectstatic *stat)
{
/*
    if (stat->historyList == NULL)
    {                                                       //if effect has no history
        return;
    }
*/
    dbgAssertOrIgnore(stat->historyList != NULL);                   //make sure effect has a history list
    if (stat->iHistoryList == stat->nHistoryList)
    {                                                       //if at end of history list
        stat->iHistoryList = 0;                             //wrap around to start
    }
    stat->historyList[stat->iHistoryList] = universe.totaltimeelapsed;
    stat->iHistoryList++;
}

/*-----------------------------------------------------------------------------
    Name        : etgFrequencyExceeded
    Description : See if an effect is being played too often
    Inputs      : stat - effect static to check
    Outputs     :
    Return      : TRUE if the effect should not be played because it's max frequency
                    is exceeded.
----------------------------------------------------------------------------*/
bool32 etgFrequencyExceeded(etgeffectstatic *stat)
{
    sdword index, max, count = 0, countThisSecond = 0;
    real32 *historyList;

    max = stat->nHistoryList;
    historyList = stat->historyList;

    if (historyList == NULL)
    {                                                       //if no history list
#if ETG_VERBOSE_LEVEL > 1
        dbgMessagef("etgFrequencyExceeded: effect '%s' has no history.", stat->name);
#endif
        return(FALSE);                                      //go ahead and make an effect
    }

    for (index = stat->iHistoryList - 1; count < max; index--, count++, countThisSecond++)
    {
        if (index < 0)
        {                                                   //if start of history list
            index = max - 1;                                //wrap around to end
        }
        if (universe.totaltimeelapsed - historyList[index] > EH_TimeElapsed)
        {                                                   //if this one was way long ago
            break;                                          //found all the instances spawned this second
        }
    }

    max = max * etgHistoryScalar / 256;
    if (countThisSecond >= max)
    {
        return(TRUE);                                       //all used, don't play
    }
    if (randyrandombetween(RANDOM_ETG, countThisSecond, max) < countThisSecond)
    {                                                       //randomly see if we should play
        return(TRUE);                                       //don't play if the number comes up
    }
    return(FALSE);                                          //you are free to play this effect
}

/*-----------------------------------------------------------------------------
    Name        : etgSpawnNewEffect
    Description : handle spawning new effects
    Inputs      : CAT: an ETG callable (etgSpawnNewEffect 0x00470c50): args[0] effect which is spawning the new
                  effect, [1] static info of the new effect, [2] number of user parameters, [3...] the user
                  parameters (etgSpawnParse builds the call)
    Outputs     : Creates a new effect using the location, velocity, etc. of
                    the specified effect and using the new effect offsets.
    Return      : pointer to newly created effect, or NULL if not created
----------------------------------------------------------------------------*/
memsize etgSpawnNewEffect(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);
    etgeffectstatic *stat = etgArgPtr(args, 1, etgeffectstatic *);
    sdword nParams = etgArgInt(args, 2);
    Effect *newEffect;
    sdword size;
    vector position, newPos;
    matrix rotTheta, rotMu, tempMatrix;
    real32 sinTheta, cosTheta, sinMu, cosMu;


    if (etgFrequencyExceeded(stat))
    {
        return(0);
    }

//    size = etgEffectSize(stat->nParticleBlocks);            //compute size of effect
    size = stat->effectSize;
    newEffect = memAlloc(size, "Ef(Effect)", Pyrophoric);       //allocate the new effect
    newEffect->obj.catMeshState = NULL;                     // CAT: +0x80 cleared, +0x84 = a one-byte block
    newEffect->obj.catEffectBlock = memAlloc(1, "EffectBlock", 0);  //      (etgSpawnNewEffect 0x00470c50)
    newEffect->header.objtype = OBJ_EffectType;
    newEffect->header.catFlags18 = 0;                      // CAT: cleared as the exe does
    if (etgEffOffset.attachParent && effect->owner != NULL)
    {
        newEffect->header.flags = SOF_Rotatable | SOF_AttachPosition | SOF_AttachVelocity;
        newEffect->owner = effect->owner;                   // no host for this effect
        effect->nSpawnedAttachedToOwner++;
    }
    else
    {
        newEffect->header.flags = SOF_Rotatable;
        newEffect->owner = NULL;                            // no host for this effect
    }
    newEffect->staticinfo = stat;
    ClearNode(newEffect->obj.renderlink);
    newEffect->obj.currentLOD = effect->obj.currentLOD;
    newEffect->obj.cameraDistanceSquared = effect->obj.cameraDistanceSquared;
    newEffect->timeElapsed = -etgEffOffset.offsetTime;      //brand new !!! how about time-offsets?

    //determine new position in space of current effect
    position.x = etgEffOffset.offsetR; position.y = 0.0f; position.z = etgEffOffset.offsetLOF;
    if (etgEffOffset.offsetR != 0.0f)
    {
        sinTheta = (real32)sin((double)etgEffOffset.offsetTheta);
        cosTheta = (real32)cos((double)etgEffOffset.offsetTheta);
        matMakeRotAboutZ(&rotTheta, cosTheta, sinTheta);
//        matMultiplyMatByMat(&rotMu, &rotTheta, &effect->rotatable.rotinfo.coordsys);
        matMultiplyVecByMat(&newPos, &position, &rotTheta);    //new radial offset
        position = newPos;
    }

    vecAdd(newPos, position, etgEffOffset.offsetXYZ);       //add in the XYZ offset
    matMultiplyMatByVec(&position, &effect->rotatable.rotinfo.coordsys, &newPos);//get offset vector in world space
    vecAdd(newEffect->obj.posinfo.position, position,             //and get position in world space
           effect->obj.posinfo.position);

    //create new coordinate system using current effects matrix and the new LOF
    //offsets from current
    if (etgEffOffset.changeLOFTheta != 0.0f || etgEffOffset.changeLOFMu != 0.0f)
    {                                                       //if there is even any offsets
        position.x = 0.0f; position.y = 0.0f; position.z = 1.0f;

        sinTheta = (real32)sin((double)etgEffOffset.changeLOFTheta);
        cosTheta = (real32)cos((double)etgEffOffset.changeLOFTheta);
        sinMu = (real32)sin((double)etgEffOffset.changeLOFMu);
        cosMu = (real32)cos((double)etgEffOffset.changeLOFMu);
        matMakeRotAboutY(&rotMu, cosMu, sinMu);
        matMakeRotAboutZ(&rotTheta, cosTheta, sinTheta);
        matMultiplyMatByMat(&tempMatrix, &rotTheta, &rotMu);//concat the two rotations
        matMultiplyMatByMat(&newEffect->rotatable.rotinfo.coordsys,   //matrix in world space
                            &effect->rotatable.rotinfo.coordsys, &tempMatrix);
    }
    else
    {
        newEffect->rotatable.rotinfo.coordsys = effect->rotatable.rotinfo.coordsys;
    }
    SET_MOVING_IMMOBILE(newEffect->obj.posinfo.isMoving);
    newEffect->obj.posinfo.haventCalculatedDist = TRUE;
    newEffect->drag = effect->drag;
//    newEffect->rotatable.rotinfo.coordsys = coordSystem;

    //create new compound velocity vector
//    position.x = 0.0f; position.y = 0.0f; position.z = etgEffOffset.offsetVelLOF;
//    matMultiplyVecByMat(&newEffect->obj.posinfo.velocity, &position, &newEffect->rotatable.rotinfo.coordsys);
//    vecAdd(newEffect->obj.posinfo.velocity,
//           newEffect->obj.posinfo.velocity, effect->obj.posinfo.velocity);
    newEffect->obj.posinfo.velocity = effect->obj.posinfo.velocity;

    univUpdateObjRotInfo((SpaceObjRot *)newEffect);

    //get the parameters
    dbgAssertOrIgnore(nParams >= 0 && nParams <= ETG_NumberParameters);
    etgEffectCodeStart(stat, newEffect, nParams, args + 3); //get the code a-runnin'

    if (bitTest(stat->specialOps, ESO_SortForward))
    {                                                       //make it sort forward, if applicable
        bitSet(newEffect->header.flags, SOF_AlwaysSortFront);
    }
    //newEffect->owner = effect->owner;                       // no host for this effect
    newEffect->nSpawnedAttachedToOwner = 0;
    newEffect->magnitudeSquared = effect->magnitudeSquared;
    newEffect->spin = etgEffOffset.spin;
    newEffect->deltaSpin = etgEffOffset.deltaSpin;
    listAddNode(&universe.SpaceObjList,&(newEffect->header.objlink),newEffect);
    listAddNode(&universe.effectList,&(newEffect->effectLink),newEffect);
    cat_univShipMeshStateCreate(&newEffect->obj);           // CAT: etgSpawnNewEffect 0x00470c50 (NULL: not a ship)
    univAddObjToRenderListIf((SpaceObj *)newEffect,(SpaceObj *)effect);// add to render list if parent ship is in render list

    return((memsize)newEffect);                              //return pointer to ETG code
}

/*-----------------------------------------------------------------------------
    adjust parameters of effects to be spawned
    CAT: ETG callables (etgEffOffsetLOF 0x00471030 ... etgEffDefaults 0x00471150); args as the script wrote them
-----------------------------------------------------------------------------*/
memsize etgEffOffsetLOF(etgarg *args)
{
    etgEffOffset.offsetLOF = etgArgFloat(args, 0);
    return 0;
}
memsize etgEffChangeLOF(etgarg *args)
{
    etgEffOffset.changeLOFTheta = etgArgFloat(args, 0);
    etgEffOffset.changeLOFMu = etgArgFloat(args, 1);
    return 0;
}
memsize etgEffOffsetR(etgarg *args)
{
    etgEffOffset.offsetR = etgArgFloat(args, 0);
    etgEffOffset.offsetTheta = etgArgFloat(args, 1);
//    etgEffOffset.offsetMu = newMu;
    return 0;
}
memsize etgEffOffsetXYZ(etgarg *args)
{
    etgEffOffset.offsetXYZ.x = etgArgFloat(args, 0);
    etgEffOffset.offsetXYZ.y = etgArgFloat(args, 1);
    etgEffOffset.offsetXYZ.z = etgArgFloat(args, 2);
    return 0;
}
memsize etgEffOffsetVelLOF(etgarg *args)
{
    etgEffOffset.offsetVelLOF = etgArgFloat(args, 0);
    return 0;
}
memsize etgEffOffsetSpin(etgarg *args)
{
    etgEffOffset.offsetSpin = etgArgFloat(args, 0);
    return 0;
}
memsize etgEffOffsetTime(etgarg *args)
{
    etgEffOffset.offsetTime = etgArgFloat(args, 0);
    return 0;
}
// CAT: 'this' first (args[0] effect, args[1] value)
memsize etgEffOffsetDrag(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);
    etgEffOffset.drag = effect->drag + 1.0f - etgArgFloat(args, 1);
    return 0;
}
memsize etgEffDrag(etgarg *args)
{
    etgEffOffset.drag = 1.0f - etgArgFloat(args, 0);
    return 0;
}
memsize etgEffAttachParent(etgarg *args)
{
    etgEffOffset.attachParent = (bool32)etgArgUdword(args, 0);
    return 0;
}
// setEffSpin/setEffDeltaSpin are declared EVT_Int in the table but read their slot as a float, as HW1 and the exe do
memsize etgEffSpin(etgarg *args)
{
    etgEffOffset.spin = etgArgFloat(args, 0);
    return 0;
}
memsize etgEffDeltaSpin(etgarg *args)
{
    etgEffOffset.deltaSpin = etgArgFloat(args, 0);
    return 0;
}
memsize etgEffDefaults(etgarg *args)
{
    memset(&etgEffOffset, 0, sizeof(etgEffOffset));
    return 0;
}
//functions for modifying the current effect ('this' first)
memsize etgThisOffsetXYZ(etgarg *args)
{
    dbgMessagef("etgThisOffsetXYZ not implemented yet!");
    return 0;
}
memsize etgThisScaleVelocity(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);
    vecMultiplyByScalar(effect->obj.posinfo.velocity, etgArgFloat(args, 1));
    dbgMessagef("etgThisScaleVelocity not implemented yet!");
    return 0;
}
//spin an effect about it's axis
memsize etgThisOffsetSpin(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);
    effect->spin = etgArgFloat(args, 1);
    return 0;
}
memsize etgThisOffsetDeltaSpin(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);
    effect->deltaSpin = etgArgFloat(args, 1);
    return 0;
}
memsize etgThisOffsetTime(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);
    effect->timeElapsed -= etgArgFloat(args, 1);
    return 0;
}
memsize etgThisSetDrag(etgarg *args)
{
    Effect *effect = etgArgPtr(args, 0, Effect *);
    effect->drag = 1.0f - etgArgFloat(args, 1);
    return 0;
}
memsize etgSetDefaults(etgarg *args)
{
    etgEffDefaults(NULL);
    partSetDefaults();
    return 0;
}
memsize etgSetDrag(etgarg *args)
{
    partSetDrag(1.0f - etgArgFloat(args, 0));
    return 0;
}
memsize etgSetDepthWrite(etgarg *args)
{
    partSetNoDepthWrite(etgArgUdword(args, 0) == 0);
    return 0;
}

memsize etgModifyDrag(etgarg *args)
{
    partModifyDrag(etgArgPtr(args, 0, psysPtr), 1.0f - etgArgFloat(args, 1));
    return 0;
}

void etgModifyDepthWrite(ubyte *sys, udword write)
{
    partModifyNoDepthWrite((psysPtr)sys, !write);
}

// CAT: the exe converts colour bytes with a multiplication by the float 1/255 (0x0072a6ac), HW1 divides by 255.0f
#define CAT_ETG_UB2R(b)     ((real32)(b) * (1.0f / 255.0f))

memsize etgSetColorA(etgarg *args)
{
    color c = etgArgColor(args, 0);
#if FIX_ENDIAN
    // color c is actually a numerically calculated RGBA udword
    // rather than properly constructed from its constituents
    c = FIX_ENDIAN_INT_32(c);
#endif

    partSetColorA(
        CAT_ETG_UB2R(colRed  (c)),
        CAT_ETG_UB2R(colGreen(c)),
        CAT_ETG_UB2R(colBlue (c)),
        CAT_ETG_UB2R(colAlpha(c))
    );
    return 0;
}

memsize etgSetColor(etgarg *args)
{
    color c = etgArgColor(args, 0);
#if FIX_ENDIAN
    // color c is actually a numerically calculated RGBA udword
    // rather than properly constructed from its constituents
    c = FIX_ENDIAN_INT_32(c);
#endif

    partSetColor(
        CAT_ETG_UB2R(colRed  (c)),
        CAT_ETG_UB2R(colGreen(c)),
        CAT_ETG_UB2R(colBlue (c))
    );
    return 0;
}

// CAT: args animation, frame rate, loop count (etgSetAnimation 0x00471370; partSetLoopFlag takes its argument array)
memsize etgSetAnimation(etgarg *args)
{
    memsize loop = (etgArgInt(args, 2) != 0);

    partSetAnimation(etgArgPtr(args, 0, void *));
    partSetFramerate(etgArgFloat(args, 1));
    partSetLoopFlag(&loop);
    return 0;
}

memsize etgSetMorphAnimation(etgarg *args)
{
    partSetMorph(etgArgPtr(args, 0, void *));
    partSetMorphFramerate(etgArgFloat(args, 1));
    partSetMorphLoopCount(etgArgInt(args, 2));
    return 0;
}

memsize etgSetSpecular(etgarg *args)
{
    real32 exponent = etgArgFloat(args, 0);

    if (exponent == 0.0f)
    {
        partSetSpecular(FALSE);
    }
    else
    {
        partSetSpecular(TRUE);
        partSetExponent(exponent);
    }
    return 0;
}

// CAT: etgFRandom 0x00471420: lo + rand * (hi - lo) / 2^32, clamped to [lo, hi] (x87, one rounding to float); HW1
//      computed in float precision and divided by UDWORD_Max
memsize etgFRandom(etgarg *args)
{
    real32 low = etgArgFloat(args, 0), high = etgArgFloat(args, 1);
    real32 value;

    if (high <= low)
    {
        return(Real32ToUdword(low));
    }
    value = (real32)((long double)ranRandom(RANDOM_ETG) * (long double)(high - low) * (1.0L / 4294967296.0L) + low);
    if (value < low)
    {
        value = low;
    }
    else if (value > high)
    {
        value = high;
    }
#if ETG_VERBOSE_LEVEL >= 2
    dbgMessagef("frandom(%f, %f) = %f", low, high, value);
#endif
    return(Real32ToUdword(value));
}

memsize etgIRandom(etgarg *args)
{
    udword low = etgArgUdword(args, 0), high = etgArgUdword(args, 1);

    if (high <= low)
    {
        return(low);
    }
//    dbgAssertOrIgnore(high > low);
    return(ranRandom(RANDOM_ETG) % (high - low) + low);
}

/*-----------------------------------------------------------------------------
    CAT: etgCRandom 0x004714e0 / etgCARandom 0x00471580, as the exe computes them: a high bound is replaced by
    low + 1 whenever low <= high (HW1's test reversed, kept by GoK); the green high bound is never read, green uses
    the (replaced) blue high bound; the channels are drawn blue, green, red (etgCRandom) and alpha, green, blue, red
    (etgCARandom), each low + rand % (high - low + 1). etgCRandom masks each channel to a byte and sets alpha 0xff;
    etgCARandom ORs the channels unmasked.
-----------------------------------------------------------------------------*/
memsize etgCRandom(etgarg *args)
{
    udword loR = etgArgUdword(args, 0), hiR = etgArgUdword(args, 1), loG = etgArgUdword(args, 2);
    udword loB = etgArgUdword(args, 4), hiB = etgArgUdword(args, 5);
    udword b, g, r;

    if (loB <= hiB)
    {
        hiB = loB + 1;
    }
    if (loR <= hiR)
    {
        hiR = loR + 1;
    }
    b = ranRandom(RANDOM_ETG) % (hiB - loB + 1) + loB;
    g = ranRandom(RANDOM_ETG) % (hiB - loG + 1) + loG;
    r = ranRandom(RANDOM_ETG) % (hiR - loR + 1) + loR;
    return((udword)(0xff000000 | ((b & 0xff) << 16) | ((g & 0xff) << 8) | (r & 0xff)));
}

memsize etgCARandom(etgarg *args)
{
    udword loR = etgArgUdword(args, 0), hiR = etgArgUdword(args, 1), loG = etgArgUdword(args, 2);
    udword loB = etgArgUdword(args, 4), hiB = etgArgUdword(args, 5), loA = etgArgUdword(args, 6), hiA = etgArgUdword(args, 7);
    udword a, g, b, r;

    if (loA <= hiA)
    {
        hiA = loA + 1;
    }
    if (loB <= hiB)
    {
        hiB = loB + 1;
    }
    if (loR <= hiR)
    {
        hiR = loR + 1;
    }
    a = ranRandom(RANDOM_ETG) % (hiA - loA + 1) + loA;
    g = ranRandom(RANDOM_ETG) % (hiB - loG + 1) + loG;
    b = ranRandom(RANDOM_ETG) % (hiB - loB + 1) + loB;
    r = ranRandom(RANDOM_ETG) % (hiR - loR + 1) + loR;
    return((udword)((a << 24) | (b << 16) | (g << 8) | r));
}

//convert data types
memsize etgFloat2Int(etgarg *args)
{
    return((udword)(sdword)etgArgFloat(args, 0));
}

memsize etgInt2Float(etgarg *args)
{
    real32 retVal = (real32)etgArgInt(args, 0);
    return Real32ToUdword(retVal);
}

// CAT: new, cat_etgColRed/Green/Blue 0x00471650/0x00471680/0x004716b0 ('colRed', 'colGreen', 'colBlue'): one channel
//      of a colour as a float in [0, 1] (byte * 1/255)
memsize cat_etgColRed(etgarg *args)
{
    real32 value = CAT_ETG_UB2R(etgArgUdword(args, 0) & 0xff);
    return(Real32ToUdword(value));
}

memsize cat_etgColGreen(etgarg *args)
{
    real32 value = CAT_ETG_UB2R((etgArgUdword(args, 0) >> 8) & 0xff);
    return(Real32ToUdword(value));
}

memsize cat_etgColBlue(etgarg *args)
{
    real32 value = CAT_ETG_UB2R((etgArgUdword(args, 0) >> 16) & 0xff);
    return(Real32ToUdword(value));
}

memsize etgInts2Color(etgarg *args)
{
    return(colRGB(etgArgInt(args, 0), etgArgInt(args, 1), etgArgInt(args, 2)));
}

memsize etgInts2ColorA(etgarg *args)
{
    return((udword)colRGBA(etgArgInt(args, 0), etgArgInt(args, 1), etgArgInt(args, 2), etgArgInt(args, 3)));
}

// CAT: the exe converts a float channel by rounding x * 256 to the nearest integer (0x0072a6a8 = 256.0, then the
//      1.5 * 2^52 rounding constant 0x0072a458), clamps it to 255 from above and keeps the low byte (etgFloats2Color
//      0x00471730, etgFloats2ColorA 0x00471810); HW1's colRealToUbyte truncated x * 255
static udword catEtgFloatToUbyte(real32 x)
{
    sdword v = (sdword)llrintl((long double)x * 256.0L);
    return (v > 0xff) ? 0xff : ((udword)v & 0xff);
}

memsize etgFloats2Color(etgarg *args)
{
    udword r = catEtgFloatToUbyte(etgArgFloat(args, 0));
    udword g = catEtgFloatToUbyte(etgArgFloat(args, 1));
    udword b = catEtgFloatToUbyte(etgArgFloat(args, 2));
    return((udword)(0xff000000 | (b << 16) | (g << 8) | r));
}

memsize etgFloats2ColorA(etgarg *args)
{
    udword r = catEtgFloatToUbyte(etgArgFloat(args, 0));
    udword g = catEtgFloatToUbyte(etgArgFloat(args, 1));
    udword b = catEtgFloatToUbyte(etgArgFloat(args, 2));
    udword a = catEtgFloatToUbyte(etgArgFloat(args, 3));
    return((udword)((a << 24) | (b << 16) | (g << 8) | r));
}

// CAT: the exe's empty function (nullsub 0x0049ae30), the table's 'print' and 'modifyBackgroundColor'
memsize cat_etgNullsub(etgarg *args)
{
    return 0;
}

/*
"ETG Fixes: Following modify functions would be very useful if added to the ETG:
X modifyVelLOF
X modifyDeltaVelLOF
X modifyVelR
X modifyDeltaVelR
? modifyVelTheta
? modifyDeltaVelTheta
X modifyMesh(ObHandle, Handle)
X modifyLength(ObHandle, Handle)"
*/

/*=============================================================================
    CAT: port diagnostics, not in the exe (docs/m2-r3-etg-particles.md):
    /catEtgParseTest <listfile>   etgStartup (the exe's 8 scripts and every effect they load), then every other
                                  etg\*.script and etg\*.ebg named in listfile; one "etgtest:" log line each
    /catEtgEffectTest <ticks>     after the parse test: create every effect of the event tables the scripts fill,
                                  with the parameters each declares, and run them for <ticks> universe ticks (code, spawned
                                  effects, particle updates; no rendering)
    The engine quits after the test (SDL/utility.c).
=============================================================================*/
char catEtgSelfTestList[PATH_MAX] = "";
sdword catEtgEffectSelfTestTicks = 0;

static bool32 catEtgSelfTestIsListedScript(char *name)
{
    sdword index;

    for (index = 0; etgScriptFilename[index] != NULL; index++)
    {
        if (strcasecmp(etgScriptFilename[index], name) == 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

static sdword catEtgSelfTestCountStatics(sdword *nPlaceholders)
{
    sdword index, nLoaded = 0;

    *nPlaceholders = 0;
    for (index = 0; index < ETG_EventListLength; index++)
    {
        if (etgEventTable[index].effectStatic != NULL)
        {
            if (etgEventTable[index].effectStatic->nParticleBlocks == 0xffffffff)
            {                                               //named by a spawn/createEffects, never loaded
                (*nPlaceholders)++;
                dbgMessagef("etgtest: referenced but not loaded: %s", etgEventTable[index].effectStatic->name);
            }
            else
            {
                nLoaded++;
            }
        }
    }
    return nLoaded;
}

void catEtgRunParseSelfTest(void)
{
    FILE *list;
    char line[PATH_MAX];
    char *end, *name;
    sdword nScripts = 0, nEffects = 0, nLoadedHere = 0, nLoaded, nPlaceholders;
    etgeffectstatic *stat;

    list = fopen(catEtgSelfTestList, "rt");
    if (list == NULL)
    {
        dbgMessagef("etgtest: cannot open list file %s", catEtgSelfTestList);
        return;
    }
    dbgMessagef("etgtest: self-test start, list %s", catEtgSelfTestList);

    //the scripts the exe loads, in its order (etgStartup 0x0046a260)
    for (nScripts = 0; etgScriptFilename[nScripts] != NULL; nScripts++)
    {
        dbgMessagef("etgtest: startup script %s", etgScriptFilename[nScripts]);
    }
    etgStartup();
    nLoaded = catEtgSelfTestCountStatics(&nPlaceholders);
    dbgMessagef("etgtest: etgStartup done, %d scripts, %d effects loaded, %d referenced but not loaded", nScripts, nLoaded, nPlaceholders);

    //every other script and effect file of the install
    while (fgets(line, sizeof(line), list) != NULL)
    {
        for (end = line + strlen(line); end > line && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' '); end--)
        {
            end[-1] = '\0';
        }
        if (line[0] == '\0' || line[0] == '#')
        {
            continue;
        }
        for (name = line + strlen(line); name > line && name[-1] != '\\' && name[-1] != '/'; name--)
        {                                                   //strip the etg\ directory
        }
        if (strlen(name) > 7 && strcasecmp(name + strlen(name) - 7, ".script") == 0)
        {
            if (!catEtgSelfTestIsListedScript(name))
            {
                dbgMessagef("etgtest: extra script %s (not loaded by the exe)", name);
                scriptSet(ETG_Directory, name, etgScriptParseTable);
                nScripts++;
            }
            continue;
        }
        if (strlen(name) > 4 && strcasecmp(name + strlen(name) - 4, ".ebg") == 0)
        {
            nEffects++;
            stat = etgEffectStaticFind(name, FALSE);
            if (stat == NULL || stat->nParticleBlocks == 0xffffffff)
            {                                               //not loaded by any script: the exe never reads it
                nLoadedHere++;                              //(some of these do not compile, e.g. an undefined label)
                dbgMessagef("etgtest: unused %s (no script loads it; not parsed)", name);
            }
            else
            {
                dbgMessagef("etgtest: ok %s blocks=%d variables=%d", name, stat->nParticleBlocks, stat->variableSize);
            }
        }
    }
    fclose(list);
    nLoaded = catEtgSelfTestCountStatics(&nPlaceholders);
    dbgMessagef("etgtest: self-test done, %d scripts parsed, %d .ebg listed, %d unused (no script loads them), %d effects loaded, %d referenced but not loaded",
                nScripts, nEffects, nLoadedHere, nLoaded, nPlaceholders);
}

/*-----------------------------------------------------------------------------
    The effect self-test passes every parameter an effect declares in its eventStart(...) line: the declared default
    where there is one, otherwise 1 (int), 10.0 (float) or white (RGB/RGBA). The effect statics keep no parameter
    types, so the declaration is read from the effect file again (test only). Up to ETG_NumberParameters.
-----------------------------------------------------------------------------*/
static sdword catEtgSelfTestParams(etgeffectstatic *stat, etgarg *params)
{
    char path[PATH_MAX], *text, *start, *end, *param, *eq, *type;
    void *buffer;
    sdword length, nParams = 0;
    real32 valueReal;
    sdword valueInt;

    sprintf(path, "%s%s", ETG_Directory, stat->name);
    length = fileLoadAlloc(path, &buffer, 0);
    text = memAlloc(length + 1, "etgtestText", 0);
    memcpy(text, buffer, length);
    text[length] = '\0';
    memFree(buffer);
    start = strstr(text, "eventStart(");
    if (start != NULL)
    {
        start += strlen("eventStart(");
        end = strchr(start, ')');
        if (end != NULL)
        {
            *end = '\0';
            for (param = strtok(start, ","); param != NULL && nParams < ETG_NumberParameters; param = strtok(NULL, ","))
            {
                while (*param == ' ' || *param == '\t')
                {
                    param++;
                }
                type = param;
                eq = strchr(param, '=');
                if (strncasecmp(type, "int", 3) == 0)
                {
                    valueInt = 1;
                    if (eq != NULL)
                    {
                        sscanf(eq + 1, "%d", &valueInt);
                    }
                    params[nParams++] = (udword)valueInt;
                }
                else if (strncasecmp(type, "float", 5) == 0)
                {
                    valueReal = 10.0f;
                    if (eq != NULL)
                    {
                        sscanf(eq + 1, "%f", &valueReal);
                    }
                    params[nParams++] = Real32ToUdword(valueReal);
                }
                else if (strncasecmp(type, "RGB", 3) == 0)
                {
                    params[nParams++] = (udword)colRGBA(255, 255, 255, 255);
                }
            }
        }
    }
    memFree(text);
    return nParams;
}

//create one effect of a lod table entry (first level, as the game does at full detail)
static sdword catEtgSelfTestCreate(etglod *lod)
{
    static vector pos = {0.0f, 0.0f, 0.0f}, vel = {0.0f, 0.0f, 100.0f};
    etgarg p[ETG_NumberParameters];
    sdword n;

    if (lod == NULL || lod->nLevels < 1 || lod->level[0] == NULL || lod->level[0]->nParticleBlocks == 0xffffffff)
    {
        return 0;
    }
    memset(p, 0, sizeof(p));
    n = catEtgSelfTestParams(lod->level[0], p);
    etgEffectCreate(lod->level[0], NULL, &pos, &vel, (matrix *)&IdentityMatrix, 1.0f, 0, n,
                    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
    return 1;
}

//the update half of etgEffectDraw: tick the particle systems of an effect (no rendering)
static void catEtgSelfTestParticles(Effect *effect)
{
    sdword index;
    matrix scaledEffectSys;
    pointSystem *part;
    real32 timeElapsed;
    vector velInverse;

    if (effect->timeElapsed < 0.0f)
    {
        return;
    }
    matCopyAndScale(&scaledEffectSys, &effect->rotatable.rotinfo.coordsys, effect->magnitudeSquared);
    matMultiplyVecByMat(&velInverse, &effect->obj.posinfo.velocity, &scaledEffectSys);
    for (index = 0; index < effect->iParticleBlock; index++)
    {
        if (effect->particleBlock[index] != NULL)
        {
            part = (pointSystem *)effect->particleBlock[index];
            timeElapsed = universe.totaltimeelapsed - part->lastUpdated;
            if (timeElapsed >= 0)
            {
                partUpdateSystem((psysPtr)effect->particleBlock[index], timeElapsed, &velInverse);
                part->lastUpdated = universe.totaltimeelapsed;
            }
        }
    }
    bitSet(effect->effectFlags, EF_Rendered);
}

void catEtgRunEffectSelfTest(sdword ticks)
{
    sdword race, gun, cls, type, tick, created = 0, deleted = 0, alive, peak = 0, nParticles, peakParticles = 0;
    sdword typeSeen[PART_LENSFLARES + 1] = {0}, block;
    Node *node, *next;
    Effect *effect;
    real32 dt = UNIVERSE_UPDATE_PERIOD;
    static Camera catEtgTestCamera;
    Camera *savedCamera = mrCamera;

    dbgMessagef("etgtest: effect self-test start, %d ticks", ticks);
    //effect sounds (playSound) pan against the main-view camera, which exists only in a game
    cameraInit(&catEtgTestCamera, 5000.0f);
    mrCamera = &catEtgTestCamera;
    //every effect of the event tables the scripts fill (each table entry once), with its declared parameters
    for (race = 0; race < NUM_RACES; race++)
    {
        for (gun = 0; gun < NUM_GUN_SOUND_TYPES; gun++)
        {
            created += catEtgSelfTestCreate(etgGunEventTable[race][gun][EGT_GunFire]);
            created += catEtgSelfTestCreate(etgGunEventTable[race][gun][EGT_GunHit]);
            created += catEtgSelfTestCreate(etgGunEventTable[race][gun][EGT_GunBullet]);
            created += catEtgSelfTestCreate(etgGunEventTable[race][gun][EGT_BulletDestroyed]);
        }
        for (cls = 0; cls < NUM_CLASSES; cls++)
        {
            for (type = 0; type < EDT_NumberExplosionTypes; type++)
            {
                created += catEtgSelfTestCreate(etgDeathEventTable[race][cls][type]);
            }
        }
        for (type = 0; type < ETG_NumberResourceTypes; type++)
        {
            created += catEtgSelfTestCreate(etgResourceEffectTable[race][type]);
        }
        created += catEtgSelfTestCreate(etgTractorBeamEffectTable[race]);
    }
    for (type = 0; type < NUM_DERELICTTYPES; type++)
    {
        for (cls = 0; cls < EDT_NumberExplosionTypes; cls++)
        {
            created += catEtgSelfTestCreate(etgDeathEventTableDerelict[type][cls]);
        }
    }
    for (cls = 0; cls < NUM_CLASSES; cls++)
    {
        for (type = 0; type < DMG_NumberDamageTypes; type++)
        {
            created += catEtgSelfTestCreate(etgDamageEffectTable[cls][type]);
        }
    }
    for (type = 0; type < EGT_NumberOfSpecialEffects; type++)
    {
        created += catEtgSelfTestCreate(etgSpecialPurposeEffectTable[type]);
    }
    created += catEtgSelfTestCreate(etgHyperspaceEffect);
    dbgMessagef("etgtest: created %d effects", created);

    //run them as univUpdate does (UnivUpdate.c), plus the particle update of etgEffectDraw
    for (tick = 0; tick < ticks; tick++)
    {
        universe.totaltimeelapsed += dt;
        universe.phystimeelapsed = dt;
        alive = nParticles = 0;
        for (node = universe.effectList.head; node != NULL; node = next)
        {
            next = node->next;
            effect = (Effect *)listGetStructOfNode(node);
            if (etgEffectUpdate(effect, dt))
            {
                etgEffectDelete(effect);
                univRemoveObjFromRenderList((SpaceObj *)effect);
                listDeleteNode(&effect->header.objlink);
                deleted++;
                continue;
            }
            catEtgSelfTestParticles(effect);
            alive++;
            nParticles += effect->iParticleBlock;
            for (block = 0; block < effect->iParticleBlock; block++)
            {                                               //tally the particle system types seen
                if (effect->particleBlock[block] != NULL && partTypeof(effect->particleBlock[block]) <= PART_LENSFLARES)
                {
                    typeSeen[partTypeof(effect->particleBlock[block])]++;
                }
            }
        }
        peak = max(peak, alive);
        peakParticles = max(peakParticles, nParticles);
        if (tick % 16 == 0 || tick == ticks - 1)
        {
            dbgMessagef("etgtest: tick %d effects alive %d particle blocks %d deleted %d", tick, alive, nParticles, deleted);
        }
    }

    //clean up what is left
    alive = 0;
    for (node = universe.effectList.head; node != NULL; node = next)
    {
        next = node->next;
        effect = (Effect *)listGetStructOfNode(node);
        etgEffectDelete(effect);
        univRemoveObjFromRenderList((SpaceObj *)effect);
        listDeleteNode(&effect->header.objlink);
        alive++;
    }
    mrCamera = savedCamera;
    dbgMessagef("etgtest: particle-system ticks by type: points %d cubes %d lines %d meshes %d billboards %d ebolts %d lensflares %d (lens flare sources on the list at the end: %d)",
                typeSeen[PART_POINTS], typeSeen[PART_CUBES], typeSeen[PART_LINES], typeSeen[PART_MESH],
                typeSeen[PART_BILLBOARD], typeSeen[PART_EBOLTS], typeSeen[PART_LENSFLARES], catRndLensFlareSourceList.num);
    dbgMessagef("etgtest: effect self-test done, %d created, %d deleted while running, %d left at the end, peak %d effects, peak %d particle blocks",
                created, deleted, alive, peak, peakParticles);
}

/*=============================================================================
    Save Game Stuff
=============================================================================*/

sdword saveEtglodGunEventToIndex(etglod *lod)
{
    sdword i,j,k;

    for (i=0;i<NUM_RACES;i++)
    {
        for (j=0;j<NUM_GUN_SOUND_TYPES;j++)
        {
            for (k=0;k<EGT_NumberGunTypes;k++)
            {
                if (lod == etgGunEventTable[i][j][k])
                {
                    return (i<<16) | (j<<8) | k;
                }
            }
        }
    }

    return -1;
}

etglod *saveIndexToEtglodGunEvent(sdword index)
{
    sdword i,j,k;

    if (index == -1)
    {
        return NULL;
    }

    i = (index>>16) & 255;
    j = (index>>8) & 255;
    k = index & 255;

    if (i < NUM_RACES && j < NUM_GUN_SOUND_TYPES && k < EGT_NumberGunTypes)
    {
        return etgGunEventTable[i][j][k];
    }

    return NULL;
}


