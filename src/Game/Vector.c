// =============================================================================
//  Vector.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created June 1997 by Gary Shaw
// =============================================================================

#include "Vector.h"

#include "Debug.h"
#include "FastMath.h"

// CAT (M2-R8 oracle-rung-physics): the x87 shape of cataclysm.exe's vector helpers. The exe evaluates them on the FPU
//      at the 53-bit precision of its control word (0x027f, every 'T' record of the GOG streams): the float operands
//      are loaded as they are, products and sums are DOUBLE, the magnitude goes to fsqrt 0x00472860 as a double
//      (catFsqrtD, FastMath.h), the reciprocal / ratio is a double, and each component is rounded to float once, at
//      its fstp. catVecMagSqD is the exe's x*x + y*y + z*z (fld z; fld y; fld x: (x*x + y*y) + z*z)
static double catVecMagSqD(const vector *a)
{
    return ((double)a->x * (double)a->x + (double)a->y * (double)a->y) + (double)a->z * (double)a->z;
}

static void catVecScaleD(vector *dst, const vector *src, double k)
{
    dst->x = (real32)(k * (double)src->x);
    dst->y = (real32)(k * (double)src->y);
    dst->z = (real32)(k * (double)src->z);
}

/*=============================================================================
    Functions:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : vecNormalize
    Description : normalizes a vector (makes it a unit vector)
    Inputs      : vector
    Outputs     : vector is normalized
    Return      :
----------------------------------------------------------------------------*/
void vecNormalize(vector *a)
{
    // CAT: the x87 shape (vecNormalize 0x00550770): the double magnitude into fsqrt, compared with 0.0f, then
    //      fdivr 1.0f (a double reciprocal) and one rounding per component
    double mag = catFsqrtD(catVecMagSqD(a));

#ifdef OPTIMIZE_VERBOSE
   // vecNormalizeCounter++;
#endif
    if (mag == 0.0)
    {
        return;
    }

    catVecScaleD(a, a, 1.0 / mag);
}

/*-----------------------------------------------------------------------------
    Name        : vecHomogenize
    Description : homogenizes an hvector (to the w == 1 plane)
    Inputs      : dst - destination vector
                  src - source hvector
    Outputs     : dst is homogenized from src
    Return      :
----------------------------------------------------------------------------*/
void vecHomogenize(vector* dst, hvector* src)
{
    real32 oneOverW;

    vecGrabVecFromHVec(*dst, *src);
    if (src->w == 0.0f || src->w == 1.0f)
    {
        return;
    }

    // CAT: 1.0f / w stays a double on the FPU, one rounding per component (vecHomogenize 0x005507d0)
    catVecScaleD(dst, dst, 1.0 / (double)src->w);
    (void)oneOverW;
}

/*-----------------------------------------------------------------------------
    Name        : vecCopyAndNormalize
    Description : copies a source vector to destination vector, and then
                  normalizes the destination vector
    Inputs      : src, dst
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void vecCopyAndNormalize(vector *src,vector *dst)
{
    // CAT: the x87 shape, no zero test (vecCopyAndNormalize 0x00550820)
    double mag = catFsqrtD(catVecMagSqD(src));

    catVecScaleD(dst, src, 1.0 / mag);
}

/*-----------------------------------------------------------------------------
    Name        : vecNormalizeToLength
    Description : Normalizes vector to given length.
    Inputs      : vector, length
    Outputs     : vector will now have length length.
    Return      :
----------------------------------------------------------------------------*/
void vecNormalizeToLength(vector *a,real32 length)
{
    // CAT: the x87 shape: fdivr length, a double ratio (vecNormalizeToLength 0x00550880)
    double mag = catFsqrtD(catVecMagSqD(a));

    catVecScaleD(a, a, (double)length / mag);
}

/*-----------------------------------------------------------------------------
    Name        : vecCapVectorSloppy
    Description : limits a vector's magnitude from growing beyond a maximum
                  magnitude by chopping x,y,z separately.
    Inputs      : vectorToCap, maxMagnitude
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void vecCapVectorSloppy(vector *vectorToCap,real32 maxMagnitude)
{
    real32 negMaxMagnitude = -maxMagnitude;

    if (vectorToCap->x > maxMagnitude)
    {
        vectorToCap->x = maxMagnitude;
    }
    else if (vectorToCap->x < negMaxMagnitude)
    {
        vectorToCap->x = negMaxMagnitude;
    }

    if (vectorToCap->y > maxMagnitude)
    {
        vectorToCap->y = maxMagnitude;
    }
    else if (vectorToCap->y < negMaxMagnitude)
    {
        vectorToCap->y = negMaxMagnitude;
    }

    if (vectorToCap->z > maxMagnitude)
    {
        vectorToCap->z = maxMagnitude;
    }
    else if (vectorToCap->z < negMaxMagnitude)
    {
        vectorToCap->z = negMaxMagnitude;
    }
}

/*-----------------------------------------------------------------------------
    Name        : vecCapVector
    Description : limits a vector's magnitude from growing beyond a maximum
                  magnitude
    Inputs      : vectorToCap, maxMagnitude
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void vecCapVector(vector *vectorToCap,real32 maxMagnitude)
{
    // CAT: the x87 shape (vecCapVector 0x00550950): the double magnitude compared with the float maximum, the double
    //      ratio, one rounding per component
    double actualMag = catFsqrtD(catVecMagSqD(vectorToCap));

    if (actualMag > (double)maxMagnitude)
    {
        catVecScaleD(vectorToCap, vectorToCap, (double)maxMagnitude / actualMag);
    }
}

/*-----------------------------------------------------------------------------
    Name        : vecCapVectorWithMag
    Description : limits a vector's magnitude from growing beyond a maximum
                  magnitude
    Inputs      : vectorToCap, maxMagnitude, actualMag
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void vecCapVectorWithMag(vector *vectorToCap,real32 maxMagnitude,real32 actualMag)
{
    real32 ratio;

    if (actualMag > maxMagnitude)
    {
        ratio = maxMagnitude / actualMag;
        vectorToCap->x *= ratio;
        vectorToCap->y *= ratio;
        vectorToCap->z *= ratio;
    }
}

/*-----------------------------------------------------------------------------
    Name        : vecCapMinVector
    Description : limits a vector's minimum magnitude from growing below a maximum
                  magnitude
    Inputs      : vectorToCap, minMagnitude
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void vecCapMinVector(vector *vectorToCap,real32 minMagnitude)
{
    // CAT: the x87 shape (vecCapMinVector 0x005509b0)
    double actualMag = catFsqrtD(catVecMagSqD(vectorToCap));

    if (actualMag < (double)minMagnitude)
    {
        catVecScaleD(vectorToCap, vectorToCap, (double)minMagnitude / actualMag);
    }
}

/*-----------------------------------------------------------------------------
    Name        : vecCapMinMaxVector
    Description : limits both the vector's minimum and maximum magnitude
    Inputs      : vectorToCap, minMagnitude, maxMagnitude
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void vecCapMinMaxVector(vector *vectorToCap,real32 minMagnitude,real32 maxMagnitude)
{
    // CAT: the x87 shape (vecCapMinMaxVector 0x00550a10): both tests on the one double magnitude
    double actualMag = catFsqrtD(catVecMagSqD(vectorToCap));

    if (actualMag < (double)minMagnitude)
    {
        catVecScaleD(vectorToCap, vectorToCap, (double)minMagnitude / actualMag);
    }

    if (actualMag > (double)maxMagnitude)
    {
        catVecScaleD(vectorToCap, vectorToCap, (double)maxMagnitude / actualMag);
    }
}

/*-----------------------------------------------------------------------------
    Name        : getVectDistSloppy
    Description : given a distance vector, gives a sloppy approximation of the distance
    Inputs      : diff
    Outputs     :
    Return      : sloppy approximation of the distance
----------------------------------------------------------------------------*/
real32 getVectDistSloppy(vector diff)
{
    diff.x = ABS(diff.x);
    diff.y = ABS(diff.y);
    diff.z = ABS(diff.z);

    if (diff.x > diff.y)
    {
        if (diff.x > diff.z)
        {
            return diff.x;
        }
        else
        {
            return diff.z;
        }
    }
    else
    {
        if (diff.y > diff.z)
        {
            return diff.y;
        }
        else
        {
            return diff.z;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : vecLineIntersectWithXYPlane
    Description : finds the point of intersection between a line and the
                  XY plane at z=zp.  The line is defined with the two points
                  linepoint1 and linepoint2.
    Inputs      : linepoint1, linepoint2, zp
    Outputs     : result, point of intersection
    Return      :
----------------------------------------------------------------------------*/
void vecLineIntersectWithXYPlane(vector *result,vector *linepoint1,vector *linepoint2,real32 zp)
{
    real32 k;

    // CAT: k stays a double on the FPU, each component rounded once (vecLineIntersectWithXYPlane 0x00550b10)
    double kd = ((double)zp - linepoint1->z) / ((double)linepoint2->z - linepoint1->z);

    result->x = (real32)(((double)linepoint2->x - linepoint1->x) * kd + linepoint1->x);
    result->y = (real32)(((double)linepoint2->y - linepoint1->y) * kd + linepoint1->y);
    result->z = zp;
    (void)k;
}

/*-----------------------------------------------------------------------------
    Name        : vecLineIntersectWithYZPlane
    Description : finds the point of intersection between a line and the
                  YZ plane at x=xp.  The line is defined with the two points
                  linepoint1 and linepoint2.
    Inputs      : linepoint1, linepoint2, xp
    Outputs     : result, point of intersection
    Return      :
----------------------------------------------------------------------------*/
void vecLineIntersectWithYZPlane(vector *result,vector *linepoint1,vector *linepoint2,real32 xp)
{
    real32 k;

    // CAT: the same x87 shape (vecLineIntersectWithYZPlane 0x00550b50)
    double kd = ((double)xp - linepoint1->x) / ((double)linepoint2->x - linepoint1->x);

    result->y = (real32)(((double)linepoint2->y - linepoint1->y) * kd + linepoint1->y);
    result->z = (real32)(((double)linepoint2->z - linepoint1->z) * kd + linepoint1->z);
    result->x = xp;
    (void)k;
}

/*-----------------------------------------------------------------------------
    Name        : vecLineIntersectWithXZPlane
    Description : finds the point of intersection between a line and the
                  XZ plane at y=yp.  The line is defined with the two points
                  linepoint1 and linepoint2.
    Inputs      : linepoint1, linepoint2, yp
    Outputs     : result, point of intersection
    Return      :
----------------------------------------------------------------------------*/
void vecLineIntersectWithXZPlane(vector *result,vector *linepoint1,vector *linepoint2,real32 yp)
{
    real32 k;

    // CAT: the same x87 shape (vecLineIntersectWithXZPlane 0x00550b90)
    double kd = ((double)yp - linepoint1->y) / ((double)linepoint2->y - linepoint1->y);

    result->x = (real32)(((double)linepoint2->x - linepoint1->x) * kd + linepoint1->x);
    result->z = (real32)(((double)linepoint2->z - linepoint1->z) * kd + linepoint1->z);
    result->y = yp;
    (void)k;
}

/*-----------------------------------------------------------------------------
    Name        : vecLineIntersectWithPlane
    Description : Compute the intersection of an arbitrary plane and an
                    arbitrary ray.
    Inputs      : Vplane - point on plane
                  Vnormal - normal to plane
                  Vline - point on line
                  Vdirection - direction of line
    Outputs     : dest - intersection point
    Return      : will generate error if there is no intersection (line is
                    paralell to plane)
----------------------------------------------------------------------------*/
void vecLineIntersectWithPlane(vector *dest, vector *Vplane, vector *Vnormal, vector *Vline, vector *Vdirection)
{
    // CAT: the x87 shape (vecLineIntersectWithPlane 0x00550bd0): (Rl - Rp) stays double, (Rl-Rp).N is
    //      (tz*nz + tx*nx) + ty*ny, V.N is (nz*dz + nx*dx) + dy*ny, t = -(the quotient) a double, each component
    //      t*V + Rl rounded once; no assertion in the exe
    double tx = (double)Vline->x - Vplane->x;
    double ty = (double)Vline->y - Vplane->y;
    double tz = (double)Vline->z - Vplane->z;
    double value = ((double)Vnormal->z * Vdirection->z + (double)Vnormal->x * Vdirection->x) + (double)Vdirection->y * Vnormal->y;
    double tScalar;

    dbgAssertOrIgnore(value != 0.0);

    //t = _-(Rl-Rp).N_
    //         V.N
    tScalar = -(((tz * Vnormal->z + tx * Vnormal->x) + ty * Vnormal->y) / value);

    //R = Rl + tV
    dest->x = (real32)(tScalar * Vdirection->x + Vline->x);
    dest->y = (real32)(tScalar * Vdirection->y + Vline->y);
    dest->z = (real32)(tScalar * Vdirection->z + Vline->z);
}

/*-----------------------------------------------------------------------------
    Name        : vecVectorsBlend
    Description : Linearly blend between two vectors.
    Inputs      : start, end - endpoints of the interpolation
                  factor - amount of end to use
    Outputs     : result - where the interpolated result is stored.
    Return      :
----------------------------------------------------------------------------*/
void vecVectorsBlend(vector *result, vector *start, vector *end, real32 factor)
{
    // CAT: the x87 shape (vecVectorsBlend 0x00550c40): 1.0f - factor stays a double, one rounding per component
    double oneMinusFactor = 1.0 - (double)factor;
    result->x = (real32)(oneMinusFactor * start->x + (double)factor * end->x);
    result->y = (real32)(oneMinusFactor * start->y + (double)factor * end->y);
    result->z = (real32)(oneMinusFactor * start->z + (double)factor * end->z);
}

/*-----------------------------------------------------------------------------
    Name        : cat_vecDistance
    Description : CAT: the distance between two points (cat_vecDistance 0x00550d60)
    Inputs      : a, b
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
real32 cat_vecDistance(vector *a, vector *b)
{
    // CAT (M2-R8 oracle-rung-physics): the x87 shape: the differences double, (dz*dz + dy*dy) + dx*dx into fsqrt as a
    //      double (cat_vecDistance 0x00550d60 returns fsqrt's double on the FPU; rounded to float at this return)
    double dx = (double)a->x - b->x;
    double dy = (double)a->y - b->y;
    double dz = (double)a->z - b->z;

    return (real32)catFsqrtD((dz * dz + dy * dy) + dx * dx);
}

/*-----------------------------------------------------------------------------
    Name        : cat_vecPlaneNormalFrom3Points
    Description : CAT: the unit normal of the plane through a, b, c: (c - b) x (a - b), left as it is when its length
                  is 0 (cat_vecPlaneNormalFrom3Points 0x00550c90)
    Inputs      : a, b, c
    Outputs     : normal
    Return      :
----------------------------------------------------------------------------*/
void cat_vecPlaneNormalFrom3Points(vector *a, vector *b, vector *c, vector *normal)
{
    // CAT (M2-R8 oracle-rung-physics): the x87 shape (cat_vecPlaneNormalFrom3Points 0x00550c90): a - b and c.z - b.z
    //      stay double, c.x - b.x and c.y - b.y are stored as floats; each normal component is rounded once at its store,
    //      the length sums the DOUBLE z component with the stored x and y ((nz*nz + ny*ny) + nx*nx), the double
    //      reciprocal scales the stored components
    double ax = (double)a->x - b->x, ay = (double)a->y - b->y, az = (double)a->z - b->z;
    real32 cx = (real32)((double)c->x - b->x), cy = (real32)((double)c->y - b->y);
    double cz = (double)c->z - b->z;
    double nz;
    double length;

    normal->x = (real32)(cz * ay - (double)cy * az);
    normal->y = (real32)(az * cx - cz * ax);
    nz = ax * cy - (double)cx * ay;
    normal->z = (real32)nz;
    length = catFsqrtD((nz * nz + (double)normal->y * normal->y) + (double)normal->x * normal->x);
    if (length != 0.0)
    {
        catVecScaleD(normal, normal, 1.0 / length);
    }
}
