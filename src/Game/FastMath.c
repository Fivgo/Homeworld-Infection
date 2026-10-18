/*=============================================================================
    Name    : FastMath.c
    Purpose : HWI-EXPERIMENT (M2-R7 oracle-rung-aiship): the exe's table square
              root, fsqrt 0x00472860 (HW1 fastmath.c fmathSqrtDouble) with its
              table filled as fmathInit 0x004728c0 does, behind the engine
              switch /oracleTableFsqrt. The default stays sqrtf (decision 23,
              the user's): FastMath.h only takes this path while the switch
              is set, for the oracle's one-experiment-per-round rule
              (docs/m2-r7-oracle-rung-aiship.md "fsqrt experiment").

    Created 2026-10-18 for Homeworld-Infection (M2-R7 oracle-rung-aiship).
    This tree is REULA-licensed, see src/COPYING.REULA.
=============================================================================*/

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "FastMath.h"

/*=============================================================================
    Data:
=============================================================================*/

bool32 catFsqrtTable = FALSE;               // /oracleTableFsqrt (default off: fsqrt is sqrtf)

#define CAT_SQRT_TAB_SIZE   16384           // the exe's table: 0x00a2e144 .. 0x00a3e144, 2 x 0x2000 dwords
#define CAT_MANT_SHIFTS     7
#define CAT_EXP_BIAS        1023u
#define CAT_EXP_SHIFTS      20
#define CAT_EXP_LSB         0x00100000u
#define CAT_MANT_MASK       0x000FFFFFu

static uint32_t catFsqrtTab[CAT_SQRT_TAB_SIZE];
static bool32 catFsqrtTabFilled = FALSE;

/*=============================================================================
    Functions:
=============================================================================*/

static double catDoubleFromHiLo(uint32_t hi, uint32_t lo)
{
    uint64_t bits = ((uint64_t)hi << 32) | lo;
    double d;
    memcpy(&d, &bits, sizeof(d));
    return d;
}

/*-----------------------------------------------------------------------------
    Name        : catFmathInit
    Description : fmathInit 0x004728c0: entry i (i < 0x2000) is the high mantissa word of sqrt(1 + i/0x2000) and
                  entry 0x2000 + i that of sqrt(2 + 2i/0x2000), each from a correctly rounded double square root (the
                  exe's fsqrt instruction at the 53-bit precision of its control word 0x027f)
----------------------------------------------------------------------------*/
static void catFmathInit(void)
{
    uint32_t i;
    uint64_t bits;
    double f;

    for (i = 0; i < CAT_SQRT_TAB_SIZE / 2; i++)
    {
        f = sqrt(catDoubleFromHiLo((i << CAT_MANT_SHIFTS) | (CAT_EXP_BIAS << CAT_EXP_SHIFTS), 0));
        memcpy(&bits, &f, sizeof(bits));
        catFsqrtTab[i] = (uint32_t)(bits >> 32) & CAT_MANT_MASK;

        f = sqrt(catDoubleFromHiLo((i << CAT_MANT_SHIFTS) | ((CAT_EXP_BIAS + 1u) << CAT_EXP_SHIFTS), 0));
        memcpy(&bits, &f, sizeof(bits));
        catFsqrtTab[i + CAT_SQRT_TAB_SIZE / 2] = (uint32_t)(bits >> 32) & CAT_MANT_MASK;
    }
    catFsqrtTabFilled = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : catFmathSqrtDouble
    Description : fsqrt 0x00472860: 0.0 (the double at 0x0072a4e8) gives 0.0; otherwise the exponent is halved (unsigned
                  arithmetic on the biased exponent less 0x3ff, as the exe's shr), its parity picks the table half, the
                  top 13 mantissa bits pick the entry, and the low dword of the argument is kept as it is
----------------------------------------------------------------------------*/
double catFmathSqrtDouble(double f)
{
    uint64_t bits;
    uint32_t hi, lo, e, m;

    if (!catFsqrtTabFilled)
    {
        catFmathInit();
    }
    if (f == 0.0)
    {
        return 0.0;
    }
    memcpy(&bits, &f, sizeof(bits));
    hi = (uint32_t)(bits >> 32);
    lo = (uint32_t)bits;
    e = (hi >> CAT_EXP_SHIFTS) - CAT_EXP_BIAS;
    m = hi & CAT_MANT_MASK;
    if (e & 1u)
    {
        m |= CAT_EXP_LSB;
    }
    e >>= 1;
    hi = catFsqrtTab[m >> CAT_MANT_SHIFTS] | ((e + CAT_EXP_BIAS) << CAT_EXP_SHIFTS);
    return catDoubleFromHiLo(hi, lo);
}
