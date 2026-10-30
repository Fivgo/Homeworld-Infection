/*=============================================================================
    Name    : FastMath.c
    Purpose : CAT: the exe's table square root, fsqrt 0x00472860 (HW1
              fastmath.c fmathSqrtDouble) with its table filled as fmathInit
              0x004728c0 does. Added behind the switch /oracleTableFsqrt in
              M2-R7 (docs/m2-r7-oracle-rung-aiship.md "fsqrt experiment"); the
              engine default since M2-R9 (the user's ruling on decision 23 /
              56 / 70, 2026-10-19). HWI-EXPERIMENT: the engine switch
              /oracleLibmSqrt clears catFsqrtTable, and FastMath.h then takes
              libm's sqrtf, for an A/B run (docs/m2-r9-oracle-rung-physics.md).

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

bool32 catFsqrtTable = TRUE;                // CAT (M2-R9): the table is the default; /oracleLibmSqrt clears it (HWI-EXPERIMENT)

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
    // CAT: fcomp with 0.0 and test ah,0x40 (C3) at 0x00472864 .. 0x0047286f: C3 is also set for an unordered compare,
    //      so a NaN argument returns 0.0 too [audit: M2-R9 oracle-rung-physics; was f == 0.0 only]
    if ((f == 0.0) || isnan(f))
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

#if defined(_WIN32) || defined(__APPLE__)        // HWI-FIX(macos): Apple's libm has no fsqrt either
/*-----------------------------------------------------------------------------
    Name        : fsqrt (Windows and macOS)
    Description : HWI-FIX(win64): glibc 2.35's C23 narrowing function float fsqrt(double), the square root correctly
                  rounded to float. The Linux build calls it from the files that do not include FastMath.h (whose
                  fsqrt macro is the table square root), through <math.h>'s declaration (_GNU_SOURCE); UCRT has no
                  such function. Every such caller passes a float, for which (float)sqrt(x) is already exact (a
                  double holds 2 x 24 + 2 bits); a double argument halfway between two floats after the double
                  rounding is settled by the sign of r * r - x, so the result equals glibc's for any argument.
                  Declared for those callers in Types.h; the parentheses keep FastMath.h's macro out.
    Inputs      : x
    Outputs     :
    Return      : sqrt(x) rounded to the nearest float
----------------------------------------------------------------------------*/
float (fsqrt)(double x)
{
    double r = sqrt(x);
    float f = (float)r;

    if (isfinite(r) && r > 0.0 && (double)f != r)
    {
        float other = (r > (double)f) ? nextafterf(f, INFINITY) : nextafterf(f, 0.0f);
        double lo = ((double)f < (double)other) ? (double)f : (double)other;
        double hi = ((double)f < (double)other) ? (double)other : (double)f;

        if (r - lo == hi - r)
        {
            double e = fma(r, r, -x);       // > 0: the true root is below r; < 0: above; 0: r is exact (ties-to-even)
            if (e > 0.0)
            {
                f = (float)lo;
            }
            else if (e < 0.0)
            {
                f = (float)hi;
            }
        }
    }
    return f;
}
#endif
