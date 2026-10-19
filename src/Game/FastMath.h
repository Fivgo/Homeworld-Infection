// =============================================================================
//  FastMath.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 9/9/1997 by gshaw
// =============================================================================

#ifndef ___FASTMATH_H
#define ___FASTMATH_H

#include "Types.h"

#include <math.h>

// HWI-EXPERIMENT (M2-R7 oracle-rung-aiship): /oracleTableFsqrt switches both to the exe's table square root
//      (fsqrt 0x00472860, HW1's (real32)fmathSqrtDouble((real32)(x)); FastMath.c) for the oracle's one fsqrt experiment
//      per round; the default is sqrtf (decision 23, the user's: never switch the default)
extern bool32 catFsqrtTable;
double catFmathSqrtDouble(double f);
#define fsqrt(x)     (catFsqrtTable ? (real32)catFmathSqrtDouble((real32)(x)) : (real32)sqrtf((real32)(x)))
#define fmathSqrt(x) (catFsqrtTable ? (real32)catFmathSqrtDouble((real32)(x)) : (real32)sqrtf((real32)(x)))
#define fmathSqrtDouble sqrt

// CAT (M2-R8 oracle-rung-physics): the exe's fsqrt 0x00472860 takes a DOUBLE (its callers fstp qword the magnitude they
//      summed on the x87 at the 53-bit precision of its control word 0x027f) and returns the double on the FPU. The
//      helpers that keep the exe's x87 shape (Vector.c, Matrix.c) call this one: with /oracleTableFsqrt the exe's table
//      on the double argument, by default (decision 23 / 56: "sqrtf") sqrtf of the argument rounded to float, widened
#define catFsqrtD(x) (catFsqrtTable ? catFmathSqrtDouble((double)(x)) : (double)sqrtf((real32)(x)))

// pi
#ifndef PI
    #define PI 3.14159265358979f
#endif

#define TWOPI   (2.0f * PI)
#define FOURPI  (4.0f * PI)

// angles
#define DEG_PER_RADIAN (360.0f / TWOPI)
#define RADIAN_PER_DEG (TWOPI / 360.0f)

#define DEG_TO_RAD(x) ((x) * RADIAN_PER_DEG)
#define RAD_TO_DEG(x) ((x) * DEG_PER_RADIAN)

// volume, area computations
#define circleCircumference(radius) ( TWOPI * (radius))
#define circleArea(radius)          (    PI * (radius) * (radius))
#define sphereArea(radius)          (FOURPI * (radius) * (radius))
#define sphereVolume(radius)        (FOURPI * (radius) * (radius) * (radius) / 3.0f)

#endif

