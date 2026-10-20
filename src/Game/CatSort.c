/*=============================================================================
    Name    : CatSort.c
    Purpose : CAT (M2-R9 oracle-rung-physics): the exe's qsort, the C runtime
              quicksort cataclysm.exe links statically (0x00671060), with its
              insertion of the largest element for short partitions (shortsort
              0x006712a0) and its byte swap (0x00671310). Every sorting function
              of the exe calls it (bobUpdateObjsInBlobCollInfo 0x00426050,
              bobUpdateExtraCollBobInfo 0x00426160, formationArrangeOptimum
              0x00481200, nisLoad 0x004d4a00, animLoadNISScript 0x00421610,
              btgZip 0x00428a40, selSelectionCompare 0x00508fb0,
              cat_smSensorRangeBlobsDraw 0x00509be0, gpTitleListLoad 0x00487440,
              spTitleListLoad 0x005040c0, trRegistryRefresh 0x00587a50). Ties
              (equal keys, or comparators that never return 0, as the blob
              sort's CompareSpaceObjCollOptimizeDistCB) come out in this
              algorithm's order: the modules of a mothership, which share its
              position, are visited by the avoidance in that order
              (docs/m2-r9-oracle-rung-physics.md).

    Created 2026-10-19 for Homeworld-Infection (M2-R9 oracle-rung-physics).
    This tree is REULA-licensed, see src/COPYING.REULA.
=============================================================================*/

#include "CatSort.h"

/*=============================================================================
    Data:
=============================================================================*/

#define CAT_QSORT_CUTOFF    8           // 0x006710b2: partitions of at most 8 elements go to shortsort
#define CAT_QSORT_STACK     30          // the exe's two stacks of partition bounds: 0x108 bytes of locals

/*=============================================================================
    Functions:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : catQsortSwap
    Description : swap 0x00671310: exchanges width bytes, nothing when both are the same element
----------------------------------------------------------------------------*/
static void catQsortSwap(char *a, char *b, size_t width)
{
    char tmp;

    if (a != b)
    {
        while (width--)
        {
            tmp = *a;
            *a++ = *b;
            *b++ = tmp;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : catQsortShort
    Description : shortsort 0x006712a0: while hi > lo, the last of the largest elements of lo..hi (comp(p, max) > 0
                  moves max forward) is swapped to hi, and hi steps back
----------------------------------------------------------------------------*/
static void catQsortShort(char *lo, char *hi, size_t width, int (*comp)(const void *, const void *))
{
    char *p;
    char *max;

    while (hi > lo)
    {
        max = lo;
        for (p = lo + width; p <= hi; p += width)
        {
            if (comp(p, max) > 0)
            {
                max = p;
            }
        }
        catQsortSwap(max, hi, width);
        hi -= width;
    }
}

/*-----------------------------------------------------------------------------
    Name        : catQsort
    Description : qsort 0x00671060: the middle element is swapped to lo and is the pivot; loguy steps up while
                  comp(loguy, lo) <= 0, higuy steps down while comp(higuy, lo) >= 0, the two are swapped until they
                  cross, the pivot goes to higuy; the smaller side is sorted next and the larger one stacked
                  (0x006711b9: higuy - 1 - lo >= hi - loguy, signed)
    Inputs      : base, num, width, comp
----------------------------------------------------------------------------*/
void catQsort(void *base, size_t num, size_t width, int (*comp)(const void *, const void *))
{
    char *lo;
    char *hi;
    char *mid;
    char *loguy;
    char *higuy;
    size_t size;
    char *lostk[CAT_QSORT_STACK];
    char *histk[CAT_QSORT_STACK];
    int stkptr;

    if ((num < 2) || (width == 0))
    {
        return;
    }
    stkptr = 0;
    lo = (char *)base;
    hi = (char *)base + width * (num - 1);

recurse:
    size = (size_t)(hi - lo) / width + 1;
    if (size <= CAT_QSORT_CUTOFF)
    {
        catQsortShort(lo, hi, width, comp);
    }
    else
    {
        mid = lo + (size / 2) * width;
        catQsortSwap(mid, lo, width);

        loguy = lo;
        higuy = hi + width;
        for (;;)
        {
            do
            {
                loguy += width;
            } while ((loguy <= hi) && (comp(loguy, lo) <= 0));
            do
            {
                higuy -= width;
            } while ((higuy > lo) && (comp(higuy, lo) >= 0));
            if (higuy < loguy)
            {
                break;
            }
            catQsortSwap(loguy, higuy, width);
        }
        catQsortSwap(lo, higuy, width);

        if ((higuy - 1 - lo) >= (hi - loguy))
        {
            if (lo + width < higuy)
            {
                lostk[stkptr] = lo;
                histk[stkptr] = higuy - width;
                ++stkptr;
            }
            if (loguy < hi)
            {
                lo = loguy;
                goto recurse;
            }
        }
        else
        {
            if (loguy < hi)
            {
                lostk[stkptr] = loguy;
                histk[stkptr] = hi;
                ++stkptr;
            }
            if (lo + width < higuy)
            {
                hi = higuy - width;
                goto recurse;
            }
        }
    }

    --stkptr;
    if (stkptr >= 0)
    {
        lo = lostk[stkptr];
        hi = histk[stkptr];
        goto recurse;
    }
}
