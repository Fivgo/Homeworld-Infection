// =============================================================================
//  CatSort.h
// =============================================================================
//  CAT (M2-R9 oracle-rung-physics): the exe's qsort (the statically linked C
//  runtime's 0x00671060, its shortsort 0x006712a0 and swap 0x00671310).
//  Created 2026-10-19 for Homeworld-Infection. This tree is REULA-licensed, see
//  src/COPYING.REULA.
// =============================================================================

#ifndef ___CATSORT_H
#define ___CATSORT_H

#include <stddef.h>

// CAT: every qsort of the game goes through this one (the exe calls 0x00671060 from all eleven of its sorting
//      functions): a comparator that ties (equal keys, or HW1's callbacks that never return 0) leaves the elements in
//      the order this algorithm gives, which the host C library's qsort does not reproduce
void catQsort(void *base, size_t num, size_t width, int (*comp)(const void *, const void *));

#endif
