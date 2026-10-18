// =============================================================================
//  B-Spline.c
//  - code for generic interpolation of beta-spline curves
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 11/29/1997 by lmoloney
// =============================================================================

#include "B-Spline.h"

#include "Debug.h"
#include "Memory.h"
#include "Vector.h"

/*=============================================================================
    Functions
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : bsStartup
    Description : Start the spline module
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void bsStartup(void)
{
#if BS_TEST
    char *fileName;
    FILE *fp;
    splinecurve *curve;
    static real32 points[] = {0.0f, 1.0f, -1.0f, -0.5f, 0.0f, -2.0f, -1.2f};
    static real32 times[] = {0.0f,  0.5f,  2.0f,  3.0f, 4.0f,  5.5f,  7.0f, 7.5f, 9.0f};
    static tcb tcbs[] = {{0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                        {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 0.0f}};
    real32 time, value = 0.0f;
    sdword point;

    fileName = filePathPrepend("b-spline.chart", FF_UserSettingsPath);
    fp = fopen(fileName, "wt");
    if (fp != NULL)
    {
        curve = bsCurveStart(7, points, times, tcbs, TRUE);
        for (time = 0.0f; value != REALlyBig; time += 0.02f)
        {
            point = curve->currentPoint;
            value = bsCurveUpdate(curve, 0.02f);
            fprintf(fp, "\nTime = %2.2f, value = %2.8f", time, value);
            if (point != curve->currentPoint)
            {
                fprintf(fp, ", point = %2.2f", curve->points[curve->currentPoint]);
            }
        }
        bsCurveDelete(curve);
        fclose(fp);
    }
#endif
}

/*-----------------------------------------------------------------------------
    Name        : bsShutdown
    Description : Shuts down the spline module
    Inputs      : void
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void bsShutdown(void)
{
    ;
}

/*-----------------------------------------------------------------------------
    Name        : bsCurveStart
    Description : Start a spline curve blending
    Inputs      : nPoints - number of points in the curve
                  points - array of floats for the points (P0 .. Pn)
                  times - array of times (between points)
                  params - array of parameters(tension, continuity, bias; currently unused)
                  bAlloc - flag indicating the times and params blocks need allocation.
                    The points block is always re-allocated.
    Outputs     : allocates memory for the data specified
    Return      : pointer to newly allocated spline block
    Note        : If the bAlloc flag is set and the points must be allocated,
                  the number of points allocated will actualle be 3 greater than
                  the number of points specified: 1 extra at the start and 2
                  extra at the end.  This is because these curves need an extra
                  control points at the extremes.  Otherwise, it is assumed that
                  this arrays passed are previously allocated by this function
                  and are of valid length.
----------------------------------------------------------------------------*/
splinecurve *bsCurveStart(sdword nPoints, real32 *points, real32 *times, tcb *params, bool32 bAlloc)
{
    splinecurve *curve;
    sdword size;

    size = sizeof(splinecurve);                             //base size
    if (bAlloc)
    {
        nPoints += 3;                                           //extra control points for blending the extremes
        size += nPoints * (sizeof(real32) * 2 + sizeof(tcb));//increase the size if we have to allocate
    }
    curve = memAlloc(size, "SplineCurve", 0);
    if (bAlloc)
    {                                                       //if arrays in this allocation
        curve->points = (real32 *)((ubyte *)curve + sizeof(splinecurve));
        curve->times = (real32 *)((ubyte *)curve->points + sizeof(real32) * nPoints);

        curve->params = (tcb *)((ubyte *)curve->times + sizeof(real32) * nPoints);

        memcpy(&curve->points[1], points, sizeof(real32) * nPoints - 3);//copy list
        curve->points[0] = curve->points[1];                //duplicate first control point
        curve->points[nPoints - 1] = curve->points[nPoints - 2] =
            curve->points[nPoints - 3];                     //and last points
        memcpy(&curve->times[1], times, sizeof(real32) * nPoints - 3);//copy list
        curve->times[nPoints - 2] = curve->times[nPoints - 2] + 1.0f;//say last segments are 1 second long
        curve->times[nPoints - 1] = curve->times[nPoints - 2] + 1.0f;

        memcpy(&curve->params[1], params, sizeof(tcb) * nPoints - 3);//copy list
        curve->params[0] = curve->params[1];                //duplicate the first and last 2 control parameter blocks
        curve->params[nPoints - 1] = curve->params[nPoints - 2] = curve->params[nPoints - 3];

    }
    else
    {                                                       //else already allocated
        curve->points = points;                             //setup pointers
        curve->times = times;
        curve->params = params;
    }
    curve->timeElapsed = 0.0f;
    curve->currentPoint = 1;                                //first two points are just the extras
    curve->nPoints = nPoints;
    return(curve);
}

/*-----------------------------------------------------------------------------
    Name        : bsCurveStartPrealloced
    Description : Start a spline curve from memory that has already been allocated.
    Inputs      : curve - pointer to curve to initialize
                  nPoints - number of points in animation
                  values - values at the keyframes
                  times - time value array
                  params - tension, continuity, bias parameters
    Outputs     : fills in curve
    Return      : void
    Note        : does not allocate or assume extra points at beginning or end of arrays.
----------------------------------------------------------------------------*/
void bsCurveStartPrealloced(splinecurve *curve, sdword nPoints, real32 *values, real32 *times, tcb *params)
{
    curve->points = values;
    curve->times = times;
    curve->params = params;
    curve->timeElapsed = 0.0f;
    curve->currentPoint = 1;                                //first two points are just the extras
    curve->nPoints = nPoints;
}

/*-----------------------------------------------------------------------------
    Name        : bsHermiteCompute
    Description : Compute some hermite coeffients.
    Inputs      : time - time within key, 0..1
    Outputs     : dest - where to store the results
    Return      :
----------------------------------------------------------------------------*/
void bsHermiteCompute(real32 time, hvector *dest)
{
   real32 t2, t3, z;

   t2 = time * time;
   t3 = time * t2;
   z = 3.0f * t2 - t3 - t3;

   dest->x = 1.0f - z;
   dest->y = z;
   dest->z = t3 - t2 - t2 + time;
   dest->w = t3 - t2;
}

/*-----------------------------------------------------------------------------
    Name        : bsCurveUpdate
    Description : Update a spline curve for a given time elapsed.
    Inputs      : curve - curve to update
                  timeElapsed - time since curve created or last updated.
    Outputs     : May modify internal vector or matrix if a control point passed
    Return      : New value of the spline function, or REALlyBig if the spline
                  has ended.
----------------------------------------------------------------------------*/
real32 bsCurveUpdate(splinecurve *curve, real32 timeElapsed)
{
    static hvector hermiteVector;
    static real32 lastTime = REALlyNegative;
    real32 unscaledTime, scaledTime, keyLength, value, pinnedTime;
    sdword currentPoint;
    tcb *key0, *key1;
    static tcb *oldKey0 = NULL;
    static real32 dd0a, dd0b, ds1a, ds1b;
    real32 adj0, adj1, delta;

    dbgAssertOrIgnore(timeElapsed >= 0.0f);
    curve->timeElapsed += timeElapsed;
    // CAT (bsCurveUpdate 0x00423040): a curve that already sits on its last key returns that key's value, and running
    //      past the end returns the last value (HW1 returned REALlyBig for both); a zero-length key between two keys
    //      with tension pins the time to the key's end
    if (curve->currentPoint == curve->nPoints - 1)
    {
        return(curve->points[curve->currentPoint]);
    }
    //find next point.  Could conceivably pass multiple control points in one frame.
    while (curve->timeElapsed > curve->times[curve->currentPoint + 1])
    {                                                       //if current control point ended
        curve->currentPoint++;                              //update the current point index
        if (curve->currentPoint >= curve->nPoints - 1)
        {                                                   //if all points used up
            return(curve->points[curve->nPoints - 1]);      //this curve all used up
        }
    }
    currentPoint = curve->currentPoint;                     //faster access to point index
    keyLength = curve->times[currentPoint + 1] - curve->times[currentPoint];
    pinnedTime = curve->timeElapsed;
    if ((keyLength <= 0.03433333337306976f) && (curve->params[currentPoint].tension != 0.0f) &&
        (curve->params[currentPoint + 1].tension != 0.0f))
    {
        // [audit: M2-R4 rest-oracle] the exe pins a LOCAL copy of the time (fVar4 in 0x00423040); curve->timeElapsed
        //      keeps the real time, so the next frame continues from it, not from the key's end
        pinnedTime = curve->times[currentPoint + 1];
    }
    if (currentPoint >= curve->nPoints)
    {
        return(REALlyBig);
    }
    unscaledTime = pinnedTime - curve->times[currentPoint];  //get time into current control point
    scaledTime = unscaledTime / (keyLength);                //scale time to 1 second range

    if (lastTime != scaledTime)
    {                                                       //if we need to recompute Hermite coefficients
        bsHermiteCompute(scaledTime, &hermiteVector);       //compute Hermite coefficients
        lastTime = scaledTime;
    }
    key0 = &curve->params[currentPoint];
    key1 = &curve->params[currentPoint + 1];
    adj0 = keyLength / ( curve->times[currentPoint + 1] - curve->times[currentPoint - 1] );
    adj1 = keyLength / ( curve->times[currentPoint + 2] - curve->times[currentPoint] );
    if (key0 != oldKey0)
    {
        dd0a = ( 1.0f - key0->tension )                     //compute numerators for the tangent vector functions later on
             * ( 1.0f + key0->continuity )
             * ( 1.0f + key0->bias );
        dd0b = ( 1.0f - key0->tension )
             * ( 1.0f - key0->continuity )
             * ( 1.0f - key0->bias );
        ds1a = ( 1.0f - key1->tension )
             * ( 1.0f - key1->continuity )
             * ( 1.0f + key1->bias );
        ds1b = ( 1.0f - key1->tension )
             * ( 1.0f + key1->continuity )
             * ( 1.0f - key1->bias );
        oldKey0 = key0;
    }
    delta = curve->points[currentPoint + 1] - curve->points[currentPoint];

    value = hermiteVector.x * curve->points[currentPoint] +
            hermiteVector.y * curve->points[currentPoint + 1] +
            hermiteVector.z * (adj0 * (dd0a * (curve->points[currentPoint] - curve->points[currentPoint - 1])
               + dd0b * delta)) +
            hermiteVector.w * (adj1 * ( ds1a * delta + ds1b *
               ( curve->points[currentPoint + 2] - curve->points[currentPoint + 1])));
    #ifdef HW_PTR_64  // Fixes the launching of ships through the big bad door.
                    // Something in the above calculation seems to get lost. :(
        if( isnan(value)){
//        dbgMessagef("NAN found!");
        return (0);
        }
    #endif
    return(value);
}

/*-----------------------------------------------------------------------------
    HWI-FEATURE(motion-interp): not in the exe. Non-advancing evaluation for the
    render-time NIS (MotionInterp.c / NIS.c). bsCurveUpdate (0x00423040) stays
    the tick-time path; these read the curve and write nothing (bsCurveUpdate's
    static caches are pure functions of their inputs and are not touched).
----------------------------------------------------------------------------*/
#define BS_JUMP_KEY_LENGTH      0.03433333337306976f    // HWI-FEATURE(motion-interp): bsCurveUpdate's short-key limit

// the Hermite / tension-continuity-bias value of key segment cp at a time (bsCurveUpdate's arithmetic); the time is
// not limited to the segment, so a time past its end extrapolates it
static real32 bsSegmentValue(splinecurve *curve, sdword currentPoint, real32 time)    // HWI-FEATURE(motion-interp)
{                                                                       // HWI-FEATURE(motion-interp)
    hvector hermite;                                                    // HWI-FEATURE(motion-interp)
    real32 keyLength, scaledTime, value;                                // HWI-FEATURE(motion-interp)
    real32 dd0a, dd0b, ds1a, ds1b, adj0, adj1, delta;                   // HWI-FEATURE(motion-interp)
    tcb *key0, *key1;                                                   // HWI-FEATURE(motion-interp)
                                                                        // HWI-FEATURE(motion-interp)
    keyLength = curve->times[currentPoint + 1] - curve->times[currentPoint];    // HWI-FEATURE(motion-interp)
    scaledTime = (time - curve->times[currentPoint]) / (keyLength);     // HWI-FEATURE(motion-interp)
    bsHermiteCompute(scaledTime, &hermite);                             // HWI-FEATURE(motion-interp)
    key0 = &curve->params[currentPoint];                                // HWI-FEATURE(motion-interp)
    key1 = &curve->params[currentPoint + 1];                            // HWI-FEATURE(motion-interp)
    adj0 = keyLength / ( curve->times[currentPoint + 1] - curve->times[currentPoint - 1] );    // HWI-FEATURE(motion-interp)
    adj1 = keyLength / ( curve->times[currentPoint + 2] - curve->times[currentPoint] );        // HWI-FEATURE(motion-interp)
    dd0a = ( 1.0f - key0->tension ) * ( 1.0f + key0->continuity ) * ( 1.0f + key0->bias );    // HWI-FEATURE(motion-interp)
    dd0b = ( 1.0f - key0->tension ) * ( 1.0f - key0->continuity ) * ( 1.0f - key0->bias );    // HWI-FEATURE(motion-interp)
    ds1a = ( 1.0f - key1->tension ) * ( 1.0f - key1->continuity ) * ( 1.0f + key1->bias );    // HWI-FEATURE(motion-interp)
    ds1b = ( 1.0f - key1->tension ) * ( 1.0f + key1->continuity ) * ( 1.0f - key1->bias );    // HWI-FEATURE(motion-interp)
    delta = curve->points[currentPoint + 1] - curve->points[currentPoint];  // HWI-FEATURE(motion-interp)
    value = hermite.x * curve->points[currentPoint] +                   // HWI-FEATURE(motion-interp)
            hermite.y * curve->points[currentPoint + 1] +               // HWI-FEATURE(motion-interp)
            hermite.z * (adj0 * (dd0a * (curve->points[currentPoint] - curve->points[currentPoint - 1])   // HWI-FEATURE(motion-interp)
               + dd0b * delta)) +                                       // HWI-FEATURE(motion-interp)
            hermite.w * (adj1 * ( ds1a * delta + ds1b *                 // HWI-FEATURE(motion-interp)
               ( curve->points[currentPoint + 2] - curve->points[currentPoint + 1])));    // HWI-FEATURE(motion-interp)
#ifdef HW_PTR_64                                                        // HWI-FEATURE(motion-interp): as bsCurveUpdate
    if (isnan(value))                                                   // HWI-FEATURE(motion-interp)
    {                                                                   // HWI-FEATURE(motion-interp)
        return (0);                                                     // HWI-FEATURE(motion-interp)
    }                                                                   // HWI-FEATURE(motion-interp)
#endif                                                                  // HWI-FEATURE(motion-interp)
    return(value);                                                      // HWI-FEATURE(motion-interp)
}                                                                       // HWI-FEATURE(motion-interp)

/*-----------------------------------------------------------------------------
    Name        : bsCurveEvalAt
    Description : HWI-FEATURE(motion-interp): the value of a curve at an
                  absolute curve time at or after its current time, without
                  advancing it: the same keys, the same pinning of a short key
                  with tension and the same arithmetic as bsCurveUpdate.
    Inputs      : curve - curve to evaluate (not modified)
                  time - absolute curve time; earlier than curve->timeElapsed is
                    treated as curve->timeElapsed
    Outputs     :
    Return      : the value bsCurveUpdate would return at that time
----------------------------------------------------------------------------*/
real32 bsCurveEvalAt(splinecurve *curve, real32 time)                   // HWI-FEATURE(motion-interp)
{                                                                       // HWI-FEATURE(motion-interp)
    real32 keyLength;                                                   // HWI-FEATURE(motion-interp)
    sdword currentPoint = curve->currentPoint;                          // HWI-FEATURE(motion-interp)
                                                                        // HWI-FEATURE(motion-interp)
    if (time < curve->timeElapsed)                                      // HWI-FEATURE(motion-interp)
    {                                                                   // HWI-FEATURE(motion-interp)
        time = curve->timeElapsed;                                      // HWI-FEATURE(motion-interp)
    }                                                                   // HWI-FEATURE(motion-interp)
    if (currentPoint == curve->nPoints - 1)                             // HWI-FEATURE(motion-interp)
    {                                                                   // HWI-FEATURE(motion-interp)
        return(curve->points[currentPoint]);                            // HWI-FEATURE(motion-interp)
    }                                                                   // HWI-FEATURE(motion-interp)
    while (time > curve->times[currentPoint + 1])                       // HWI-FEATURE(motion-interp)
    {                                                                   // HWI-FEATURE(motion-interp)
        currentPoint++;                                                 // HWI-FEATURE(motion-interp)
        if (currentPoint >= curve->nPoints - 1)                         // HWI-FEATURE(motion-interp)
        {                                                               // HWI-FEATURE(motion-interp)
            return(curve->points[curve->nPoints - 1]);                  // HWI-FEATURE(motion-interp)
        }                                                               // HWI-FEATURE(motion-interp)
    }                                                                   // HWI-FEATURE(motion-interp)
    keyLength = curve->times[currentPoint + 1] - curve->times[currentPoint];    // HWI-FEATURE(motion-interp)
    if ((keyLength <= BS_JUMP_KEY_LENGTH) && (curve->params[currentPoint].tension != 0.0f) &&    // HWI-FEATURE(motion-interp)
        (curve->params[currentPoint + 1].tension != 0.0f))              // HWI-FEATURE(motion-interp)
    {                                                                   // HWI-FEATURE(motion-interp)
        time = curve->times[currentPoint + 1];                          // HWI-FEATURE(motion-interp): the pinned time
    }                                                                   // HWI-FEATURE(motion-interp)
    return bsSegmentValue(curve, currentPoint, time);                   // HWI-FEATURE(motion-interp)
}                                                                       // HWI-FEATURE(motion-interp)

/*-----------------------------------------------------------------------------
    Name        : bsCurveEvalAtBeforeJump
    Description : HWI-FEATURE(motion-interp): bsCurveEvalAt, except that a jump
                  (a key shorter than a 30 fps frame, the NIS camera cuts and
                  object re-placements) starting after the curve's current time
                  and at or before the given time is not taken: the segment
                  before it is continued past its end instead (held at its end
                  when it is short itself or the continuation would exceed half
                  of it), so the jump still shows at the tick that takes it, as
                  without interpolation, and the frames before it keep moving.
    Inputs      : curve - curve to evaluate (not modified)
                  time - absolute curve time (at most a tick past its current time)
    Outputs     :
    Return      : the value
----------------------------------------------------------------------------*/
real32 bsCurveEvalAtBeforeJump(splinecurve *curve, real32 time)         // HWI-FEATURE(motion-interp)
{                                                                       // HWI-FEATURE(motion-interp)
    sdword k;                                                           // HWI-FEATURE(motion-interp)
    real32 segment;                                                     // HWI-FEATURE(motion-interp)
                                                                        // HWI-FEATURE(motion-interp)
    for (k = curve->currentPoint; k < curve->nPoints - 2 && curve->times[k] < time; k++)   // HWI-FEATURE(motion-interp)
    {                                                                   // HWI-FEATURE(motion-interp)
        if (curve->times[k] < curve->timeElapsed || (curve->times[k + 1] - curve->times[k]) > BS_JUMP_KEY_LENGTH)   // HWI-FEATURE(motion-interp)
        {                                                               // HWI-FEATURE(motion-interp)
            continue;                                                   // HWI-FEATURE(motion-interp): no jump ahead here
        }                                                               // HWI-FEATURE(motion-interp)
        if (k < 2)                                                      // HWI-FEATURE(motion-interp): no segment before it
        {                                                               // HWI-FEATURE(motion-interp)
            return bsCurveEvalAt(curve, curve->times[k]);               // HWI-FEATURE(motion-interp)
        }                                                               // HWI-FEATURE(motion-interp)
        segment = curve->times[k] - curve->times[k - 1];                // HWI-FEATURE(motion-interp)
        if (segment <= BS_JUMP_KEY_LENGTH || (time - curve->times[k]) > 0.5f * segment)   // HWI-FEATURE(motion-interp)
        {                                                               // HWI-FEATURE(motion-interp)
            return bsSegmentValue(curve, k - 1, curve->times[k]);       // HWI-FEATURE(motion-interp): held at its end
        }                                                               // HWI-FEATURE(motion-interp)
        return bsSegmentValue(curve, k - 1, time);                      // HWI-FEATURE(motion-interp): continued
    }                                                                   // HWI-FEATURE(motion-interp)
    return bsCurveEvalAt(curve, time);                                  // HWI-FEATURE(motion-interp)
}                                                                       // HWI-FEATURE(motion-interp)

/*-----------------------------------------------------------------------------
    Name        : bsCurveDelete
    Description : Delete a spline and all it's lists, if applicable
    Inputs      : curve - curve to delete
    Outputs     :
    Return      :
    Note        : If other curves are using this one's control point lists,
                    you'd better free them appropriately.
----------------------------------------------------------------------------*/
void bsCurveDelete(splinecurve *curve)
{
    memFree(curve);
}

/*-----------------------------------------------------------------------------
    Name        : bsCurveRestart
    Description : Restarts a curve at time = 0
    Inputs      : curve - curve to restart
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void bsCurveRestart(splinecurve *curve)
{
    curve->timeElapsed = 0.0f;                              //reset time to zero
    curve->currentPoint = 1;                                //first point is just an extra
}

