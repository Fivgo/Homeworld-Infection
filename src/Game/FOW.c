// =============================================================================
//  FOW.c
//  - Cataclysm's fog of war: a coarse 3D grid of cells each player's ships
//    light up, the explored/occupied boundary drawn in the sensors manager,
//    and the visibility tests (cataclysm.exe 0x0047e7f0-0x00480d50)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file. The exe links these functions between FastMath/Formation and Gun.c with no HW1 sibling, so they came
//      from a Cataclysm source file of their own; the bodies follow the r6 decompile and disassembly of each (address
//      on each function). The exe also keeps two debug error counters (0x00a3e2ec, 0x00a3e2e8) that every function
//      sets and nothing reads (a stripped debug print); they are not ported.

#include "FOW.h"

#include <string.h>

#include "Camera.h"
#include "Color.h"
#include "Debug.h"
#include "glinc.h"
#include "LinkedList.h"
#include "Memory.h"
#include "Sensors.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "Universe.h"

#include "ClassDefs.h"
#include "ShipDefs.h"

/*=============================================================================
    Data:
=============================================================================*/

static CatFOWGrid *fowCurrentGrid = NULL;                   // 0x00a3e2f0

// the boundary renderer: one face record per cell per mode (0x44 bytes in the exe: a list node, the 12 edge flags
// and the cell index), the cells queued for a rebuild, the faces on show and the 16 distance colours
typedef struct CatFOWFace
{
    Node link;
    sdword edge[12];        // 1: the cell edge is drawn
    sdword cell;            // index of the cell
} CatFOWFace;

typedef struct CatFOWDirty
{
    sdword x, y, z;
} CatFOWDirty;

// [0] occupied (mode 2: faces 0x00a3e2f4, queue 0x00a3e300/0x00a3e304, list 0x00a3e238, colours 0x00a3e2a4),
// [1] visited (mode 1: 0x00a3e2f8, 0x00a3e308/0x00a3e30c, 0x00a3e298, colours 0x00a3e1f8),
// [2] visited and empty (mode 0: 0x00a3e2fc, 0x00a3e310/0x00a3e314, 0x00a3e288, colours 0x00a3e244)
typedef struct CatFOWBoundary
{
    CatFOWFace *faces;
    CatFOWDirty *dirty;
    sdword numDirty;
    LinkedList faceList;
    color colours[16];
} CatFOWBoundary;

#define FOW_BOUNDARY_OCCUPIED   0       // mode 2
#define FOW_BOUNDARY_VISITED    1       // mode 1
#define FOW_BOUNDARY_EMPTY      2       // mode 0

static CatFOWBoundary fowBoundary[3];

// the colour ramps (near .. far), built once on the first draw (0x0087f7c4)
static bool32 fowColoursDirty = TRUE;
static const ubyte fowOccupiedNear[3] = { 0, 0, 50 },   fowOccupiedFar[3] = { 0, 0, 255 };      // 0x0087f7a8..0x0087f7bc
static const ubyte fowVisitedNear[3]  = { 50, 50, 50 }, fowVisitedFar[3]  = { 200, 200, 200 };  // 0x0087f778..0x0087f78c
static const ubyte fowEmptyNear[3]    = { 50, 50, 50 }, fowEmptyFar[3]    = { 200, 200, 200 };  // 0x0087f790..0x0087f7a4

// the corner markers and the gap left at a marked corner: 0.2 of a cell (0x0087f7c0)
#define FOW_CORNER_FRACTION     0.2f
static vector fowCornerLines[8][3];                         // 0x00a3e328, rebuilt on each draw

// the line batch: two points and their distances from the sensors camera (0x20 bytes each in the exe)
typedef struct CatFOWLine
{
    vector p0, p1;
    real32 d0, d1;
} CatFOWLine;
static CatFOWLine *fowLines = NULL;                         // 0x00a3e318
static sdword fowNumLines = 0;                              // 0x00a3e31c
static sdword fowMaxLines = 0;                              // 0x00a3e320

// for each of a cell's 12 edges, the three cells across it (the two side neighbours and the diagonal one), as grid
// offsets (0x0072a878)
static const sdword fowEdgeNeighbours[12][3][3] =
{
    { { 0,-1, 0}, { 0,-1, 1}, { 0, 0, 1} },
    { { 0,-1, 0}, { 1,-1, 0}, { 1, 0, 0} },
    { { 0,-1, 0}, { 0,-1,-1}, { 0, 0,-1} },
    { { 0,-1, 0}, {-1,-1, 0}, {-1, 0, 0} },
    { { 0, 1, 0}, { 0, 1, 1}, { 0, 0, 1} },
    { { 0, 1, 0}, { 1, 1, 0}, { 1, 0, 0} },
    { { 0, 1, 0}, { 0, 1,-1}, { 0, 0,-1} },
    { { 0, 1, 0}, {-1, 1, 0}, {-1, 0, 0} },
    { {-1, 0, 0}, {-1, 0, 1}, { 0, 0, 1} },
    { { 0, 0, 1}, { 1, 0, 1}, { 1, 0, 0} },
    { { 1, 0, 0}, { 1, 0,-1}, { 0, 0,-1} },
    { { 0, 0,-1}, {-1, 0,-1}, {-1, 0, 0} },
};

// the corners each edge joins (0x0072aa28) and the edges at each corner (0x0072aa88)
static const sdword fowEdgeCorners[12][2] =
{
    {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4}, {4,0}, {5,1}, {6,2}, {7,3}
};
static const sdword fowCornerEdges[8][3] =
{
    {0,3,8}, {0,9,1}, {1,10,2}, {2,3,11}, {8,4,7}, {4,9,5}, {5,10,6}, {6,7,11}
};

/*=============================================================================
    Private helpers:
=============================================================================*/

#define fowCellIndex(g, x, y, z)    ((((g)->ny * (z)) + (y)) * (g)->nx + (x))
#define fowInGrid(g, x, y, z)       (((x) >= 0) && ((x) < (g)->nx) && ((y) >= 0) && ((y) < (g)->ny) && ((z) >= 0) && ((z) < (g)->nz))

// CAT: the exe's world-to-cell conversion (inlined in fowActivateCubeW, fowGetUsageCountW, cat_fowCanPlayerSeeShip,
//      ...): x87 (p - origin) * (1.0 / cellSize), truncated by __ftol; double keeps the extended-precision product
static sdword fowToCell(real32 p, real32 origin, real32 cellSize)
{
    return (sdword)(((double)p - (double)origin) * (1.0 / (double)cellSize));
}

static sdword fowToCellRadius(real32 r, real32 cellSize)
{
    return (sdword)((double)r * (1.0 / (double)cellSize)) + 1;
}

// CAT-FIX: a cube's cell box x - r / x + r in the exe's 32-bit two's complement (fowActivateCube 0x0047eeb0,
//      fowDeactivateCube 0x0047f070, fowCubeHasUnvisited 0x0047f400: sub / add that wrap). A position the exe's __ftol
//      cannot convert (NaN or out of range) gives the cell 0x80000000 there and here (cvttsd2si); x - r then wraps to
//      a large positive x0 and x + r stays negative, so the clipped box is empty in both. C's signed overflow is
//      undefined, so the wrap is done in unsigned arithmetic (UBSan's FOW.c:296-437 reports in Mission17)
#define fowCellSub(a, b)    ((sdword)((udword)(a) - (udword)(b)))
#define fowCellAdd(a, b)    ((sdword)((udword)(a) + (udword)(b)))

static void fowCellMarkDirty(sdword x, sdword y, sdword z, bool32 neighbours);
static void cat_fowBoundaryDestroy(void);

/*-----------------------------------------------------------------------------
    Name        : cat_fowGridSize
    Return      : the bytes of a grid's cells (the FOW saves size their chunks by it)
----------------------------------------------------------------------------*/
sdword cat_fowGridSize(sdword nx, sdword ny, sdword nz)     // 0x0047e7f0
{
    return nx * ny * nz * (sdword)sizeof(CatFOWCell);
}

/*-----------------------------------------------------------------------------
    Name        : fowCreate
    Description : makes a grid of 10000-unit cells over the box and selects it
    Inputs      : boxMin, boxMax - opposite corners
    Return      : the grid, or NULL
----------------------------------------------------------------------------*/
void *fowCreate(vector *boxMin, vector *boxMax)             // 0x0047e9a0
{
    CatFOWGrid *grid;
    vector low, high;
    real32 cellSize = FOW_DEFAULT_CELL_SIZE;
    real32 half;
    sdword x, y, z, numCells;
    CatFOWCell *cell;

    if ((boxMin == NULL) || (boxMax == NULL))
    {
        return NULL;
    }

    grid = memAlloc(sizeof(CatFOWGrid), "fowGrid", NonVolatile);
    memset(grid, 0, sizeof(CatFOWGrid));

    // the low corner and the size, per axis (the exe compares with fcomp, so equal coordinates take boxMin)
    if (boxMin->x < boxMax->x) { low.x = boxMin->x; high.x = boxMax->x; } else { low.x = boxMax->x; high.x = boxMin->x; }
    if (boxMin->y < boxMax->y) { low.y = boxMin->y; high.y = boxMax->y; } else { low.y = boxMax->y; high.y = boxMin->y; }
    if (boxMin->z < boxMax->z) { low.z = boxMin->z; high.z = boxMax->z; } else { low.z = boxMax->z; high.z = boxMin->z; }

    fowCurrentGrid = grid;
    grid->origin = low;
    grid->cells = NULL;
    grid->cellSize = cellSize;
    grid->nx = (sdword)((double)(high.x - low.x) / (double)cellSize);
    grid->ny = (sdword)((double)(high.y - low.y) / (double)cellSize);
    grid->nz = (sdword)((double)(high.z - low.z) / (double)cellSize);

    numCells = grid->nx * grid->ny * grid->nz;
    grid->cells = memAlloc(numCells * (sdword)sizeof(CatFOWCell), "fowCells", NonVolatile);
    memset(grid->cells, 0, numCells * sizeof(CatFOWCell));

    cat_fowBoundaryCreate(FALSE);
    fowClear();

    half = grid->cellSize * 0.5f;
    for (x = 0; x < grid->nx; x++)
    {
        for (y = 0; y < grid->ny; y++)
        {
            for (z = 0; z < grid->nz; z++)
            {
                cell = &grid->cells[fowCellIndex(grid, x, y, z)];
                cell->centre.x = (real32)x * grid->cellSize + grid->origin.x + half;
                cell->centre.y = (real32)y * grid->cellSize + grid->origin.y + half;
                cell->centre.z = (real32)z * grid->cellSize + grid->origin.z + half;
            }
        }
    }
    return grid;
}

/*-----------------------------------------------------------------------------
    Name        : fowDestroy
    Description : frees a grid and the boundary renderer's buffers; nothing is selected after
----------------------------------------------------------------------------*/
void fowDestroy(void *grid)                                 // 0x0047ed30
{
    CatFOWGrid *g = (CatFOWGrid *)grid;

    fowCurrentGrid = NULL;
    if (g != NULL)
    {
        if (g->cells != NULL)
        {
            memFree(g->cells);
        }
        memFree(g);
    }
    cat_fowBoundaryDestroy();
}

/*-----------------------------------------------------------------------------
    Name        : fowSelect
    Description : makes a grid the one the other functions work on
    Return      : the previous one
----------------------------------------------------------------------------*/
void *fowSelect(void *grid)                                 // 0x0047ed70
{
    void *previous = fowCurrentGrid;
    fowCurrentGrid = (CatFOWGrid *)grid;
    return previous;
}

/*-----------------------------------------------------------------------------
    Name        : fowClear
    Description : forgets every cube and every visit and restarts the boundary
----------------------------------------------------------------------------*/
void fowClear(void)                                         // 0x0047ed80
{
    sdword index, numCells;

    if (fowCurrentGrid == NULL)
    {
        return;
    }
    numCells = fowCurrentGrid->nx * fowCurrentGrid->ny * fowCurrentGrid->nz;
    for (index = 0; index < numCells; index++)
    {
        fowCurrentGrid->cells[index].usage = 0;
        fowCurrentGrid->cells[index].visited = 0;
    }
    cat_fowBoundaryDestroy();
    cat_fowBoundaryCreate(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : fowActivate
    Description : one more cube covers the cell, which is now visited
----------------------------------------------------------------------------*/
void fowActivate(sdword x, sdword y, sdword z)              // 0x0047edf0
{
    CatFOWCell *cell;

    // CAT-FIX: the exe range-checks against the grid before its NULL test; the NULL test comes first here
    if ((fowCurrentGrid == NULL) || !fowInGrid(fowCurrentGrid, x, y, z))
    {
        return;
    }
    fowCellMarkDirty(x, y, z, TRUE);
    cell = &fowCurrentGrid->cells[fowCellIndex(fowCurrentGrid, x, y, z)];
    cell->usage++;
    cell->visited = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : fowActivateCube
    Description : activates the cells of the cube of half-size r about (x,y,z), clipped to the grid
----------------------------------------------------------------------------*/
void fowActivateCube(sdword x, sdword y, sdword z, sdword r)    // 0x0047eeb0
{
    sdword x0, x1, y0, y1, z0, z1, i, j, k;

    // CAT-FIX: the exe only bumps its error counter and goes on to read the NULL grid's size [audit: M2-R6 nis-fow-camera]
    if (fowCurrentGrid == NULL)
    {
        return;
    }
    x0 = fowCellSub(x, r); x1 = fowCellAdd(x, r);           // CAT-FIX: the exe's wrapping arithmetic (see fowCellSub)
    y0 = fowCellSub(y, r); y1 = fowCellAdd(y, r);
    z0 = fowCellSub(z, r); z1 = fowCellAdd(z, r);
    if (x0 < 0) x0 = 0;
    if (x1 >= fowCurrentGrid->nx) x1 = fowCurrentGrid->nx - 1;
    if (y0 < 0) y0 = 0;
    if (y1 >= fowCurrentGrid->ny) y1 = fowCurrentGrid->ny - 1;
    if (z0 < 0) z0 = 0;
    if (z1 >= fowCurrentGrid->nz) z1 = fowCurrentGrid->nz - 1;

    for (k = z0; k <= z1; k++)
    {
        for (j = y0; j <= y1; j++)
        {
            for (i = x0; i <= x1; i++)
            {
                fowActivate(i, j, k);
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : fowActivateCubeW
    Description : fowActivateCube for a world position and radius (the radius in cells is rounded up by one)
----------------------------------------------------------------------------*/
// CAT: the exe passes a third argument on to fowActivateCube's unused fifth (its callers pass garbage)
void fowActivateCubeW(vector pos, real32 radius)            // 0x0047efb0
{
    if (fowCurrentGrid == NULL)
    {
        return;
    }
    fowActivateCube(fowToCell(pos.x, fowCurrentGrid->origin.x, fowCurrentGrid->cellSize),
                    fowToCell(pos.y, fowCurrentGrid->origin.y, fowCurrentGrid->cellSize),
                    fowToCell(pos.z, fowCurrentGrid->origin.z, fowCurrentGrid->cellSize),
                    fowToCellRadius(radius, fowCurrentGrid->cellSize));
}

/*-----------------------------------------------------------------------------
    Name        : fowDeactivateCube
    Description : one cube less covers each cell of the cube (a cell already at 0 stays there)
----------------------------------------------------------------------------*/
void fowDeactivateCube(sdword x, sdword y, sdword z, sdword r)  // 0x0047f070
{
    sdword x0, x1, y0, y1, z0, z1, i, j, k;
    CatFOWCell *cell;

    // CAT-FIX: the exe only bumps its error counter and goes on to read the NULL grid's size [audit: M2-R6 nis-fow-camera]
    if (fowCurrentGrid == NULL)
    {
        return;
    }
    x0 = fowCellSub(x, r); x1 = fowCellAdd(x, r);           // CAT-FIX: the exe's wrapping arithmetic (see fowCellSub)
    y0 = fowCellSub(y, r); y1 = fowCellAdd(y, r);
    z0 = fowCellSub(z, r); z1 = fowCellAdd(z, r);
    if (x0 < 0) x0 = 0;
    if (x1 >= fowCurrentGrid->nx) x1 = fowCurrentGrid->nx - 1;
    if (y0 < 0) y0 = 0;
    if (y1 >= fowCurrentGrid->ny) y1 = fowCurrentGrid->ny - 1;
    if (z0 < 0) z0 = 0;
    if (z1 >= fowCurrentGrid->nz) z1 = fowCurrentGrid->nz - 1;

    for (k = z0; k <= z1; k++)
    {
        for (j = y0; j <= y1; j++)
        {
            for (i = x0; i <= x1; i++)
            {
                if (fowInGrid(fowCurrentGrid, i, j, k))
                {
                    cell = &fowCurrentGrid->cells[fowCellIndex(fowCurrentGrid, i, j, k)];
                    if (cell->usage > 0)
                    {
                        cell->usage--;
                        fowCellMarkDirty(i, j, k, TRUE);
                    }
                }
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : fowDeactivateCubeW
    Description : fowDeactivateCube for a world position and radius
----------------------------------------------------------------------------*/
void fowDeactivateCubeW(vector pos, real32 radius)          // 0x0047f1d0
{
    if (fowCurrentGrid == NULL)
    {
        return;
    }
    fowDeactivateCube(fowToCell(pos.x, fowCurrentGrid->origin.x, fowCurrentGrid->cellSize),
                      fowToCell(pos.y, fowCurrentGrid->origin.y, fowCurrentGrid->cellSize),
                      fowToCell(pos.z, fowCurrentGrid->origin.z, fowCurrentGrid->cellSize),
                      fowToCellRadius(radius, fowCurrentGrid->cellSize));
}

/*-----------------------------------------------------------------------------
    Name        : fowGetUsageCount
    Return      : how many cubes cover the cell, or FOW_OUTSIDE_GRID
----------------------------------------------------------------------------*/
sdword fowGetUsageCount(sdword x, sdword y, sdword z)       // 0x0047f290
{
    // CAT-FIX: NULL test first (see fowActivate)
    if ((fowCurrentGrid == NULL) || !fowInGrid(fowCurrentGrid, x, y, z))
    {
        return FOW_OUTSIDE_GRID;
    }
    return fowCurrentGrid->cells[fowCellIndex(fowCurrentGrid, x, y, z)].usage;
}

sdword fowGetUsageCountW(vector pos)                        // 0x0047f300
{
    // CAT-FIX: the exe range-tests against the NULL grid after its error counter [audit: M2-R6 nis-fow-camera]
    if (fowCurrentGrid == NULL)
    {
        return FOW_OUTSIDE_GRID;
    }
    return fowGetUsageCount(fowToCell(pos.x, fowCurrentGrid->origin.x, fowCurrentGrid->cellSize),
                            fowToCell(pos.y, fowCurrentGrid->origin.y, fowCurrentGrid->cellSize),
                            fowToCell(pos.z, fowCurrentGrid->origin.z, fowCurrentGrid->cellSize));
}

/*-----------------------------------------------------------------------------
    Name        : fowCubeHasUnvisited
    Return      : TRUE if a cell of the cube (clipped to the grid) was never visited
----------------------------------------------------------------------------*/
bool32 fowCubeHasUnvisited(sdword x, sdword y, sdword z, sdword r)  // 0x0047f400
{
    sdword x0, x1, y0, y1, z0, z1, i, j, k;
    bool32 unvisited = FALSE;

    // CAT-FIX: the exe only bumps its error counter and goes on to read the NULL grid's size [audit: M2-R6 nis-fow-camera]
    if (fowCurrentGrid == NULL)
    {
        return FALSE;
    }
    x0 = fowCellSub(x, r); x1 = fowCellAdd(x, r);           // CAT-FIX: the exe's wrapping arithmetic (see fowCellSub)
    y0 = fowCellSub(y, r); y1 = fowCellAdd(y, r);
    z0 = fowCellSub(z, r); z1 = fowCellAdd(z, r);
    if (x0 < 0) x0 = 0;
    if (x1 >= fowCurrentGrid->nx) x1 = fowCurrentGrid->nx - 1;
    if (y0 < 0) y0 = 0;
    if (y1 >= fowCurrentGrid->ny) y1 = fowCurrentGrid->ny - 1;
    if (z0 < 0) z0 = 0;
    if (z1 >= fowCurrentGrid->nz) z1 = fowCurrentGrid->nz - 1;

    for (k = z0; k <= z1; k++)
    {
        for (j = y0; j <= y1; j++)
        {
            for (i = x0; i <= x1; i++)
            {
                if (fowInGrid(fowCurrentGrid, i, j, k) &&
                    (fowCurrentGrid->cells[fowCellIndex(fowCurrentGrid, i, j, k)].visited == 0))
                {
                    unvisited = TRUE;
                }
            }
        }
    }
    return unvisited;
}

/*-----------------------------------------------------------------------------
    Name        : fowGetVisited
    Return      : the cell's visited flag, or FOW_OUTSIDE_GRID
----------------------------------------------------------------------------*/
sdword fowGetVisited(sdword x, sdword y, sdword z)          // 0x0047f540
{
    // CAT-FIX: NULL test first (the exe range-tests against the grid first, see fowActivate) [audit: M2-R6 nis-fow-camera]
    if ((fowCurrentGrid == NULL) || !fowInGrid(fowCurrentGrid, x, y, z))
    {
        return FOW_OUTSIDE_GRID;
    }
    return fowCurrentGrid->cells[fowCellIndex(fowCurrentGrid, x, y, z)].visited;
}

sdword fowGetVisitedW(vector pos)                           // 0x0047f5b0
{
    // CAT-FIX: the exe range-tests against the NULL grid after its error counter [audit: M2-R6 nis-fow-camera]
    if (fowCurrentGrid == NULL)
    {
        return FOW_OUTSIDE_GRID;
    }
    return fowGetVisited(fowToCell(pos.x, fowCurrentGrid->origin.x, fowCurrentGrid->cellSize),
                         fowToCell(pos.y, fowCurrentGrid->origin.y, fowCurrentGrid->cellSize),
                         fowToCell(pos.z, fowCurrentGrid->origin.z, fowCurrentGrid->cellSize));
}

/*-----------------------------------------------------------------------------
    Name        : fowWorldToGrid
    Description : a world position (and radius) in cells; any output may be NULL
----------------------------------------------------------------------------*/
void fowWorldToGrid(real32 x, real32 y, real32 z, real32 r, sdword *gx, sdword *gy, sdword *gz, sdword *gr)  // 0x0047f6b0
{
    // CAT-FIX: the exe converts through the NULL grid after its error counter; here the outputs are left as they are [audit: M2-R6 nis-fow-camera]
    if (fowCurrentGrid == NULL)
    {
        return;
    }
    if (gx != NULL) *gx = fowToCell(x, fowCurrentGrid->origin.x, fowCurrentGrid->cellSize);
    if (gy != NULL) *gy = fowToCell(y, fowCurrentGrid->origin.y, fowCurrentGrid->cellSize);
    if (gz != NULL) *gz = fowToCell(z, fowCurrentGrid->origin.z, fowCurrentGrid->cellSize);
    if (gr != NULL) *gr = fowToCellRadius(r, fowCurrentGrid->cellSize);
}

/*-----------------------------------------------------------------------------
    Name        : fowIsValidW
    Return      : TRUE if the world position is inside the grid
----------------------------------------------------------------------------*/
bool32 fowIsValidW(real32 x, real32 y, real32 z)            // 0x0047f750
{
    CatFOWGrid *g = fowCurrentGrid;

    // CAT-FIX: the exe tests the NULL grid's box after its error counter [audit: M2-R6 nis-fow-camera]
    if (g == NULL)
    {
        return FALSE;
    }
    return (g->origin.x <= x) && (x < (real32)g->nx * g->cellSize + g->origin.x) &&
           (g->origin.y <= y) && (y < (real32)g->ny * g->cellSize + g->origin.y) &&
           (g->origin.z <= z) && (z < (real32)g->nz * g->cellSize + g->origin.z);
}

/*=============================================================================
    The boundary renderer:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : cat_fowBoundaryCreate
    Description : allocates the three modes' face records and rebuild queues for the selected grid
    Inputs      : markAllDirty - queue every cell (after a load)
----------------------------------------------------------------------------*/
void cat_fowBoundaryCreate(bool32 markAllDirty)             // 0x0047f850
{
    sdword numCells, mode, x, y, z;
    CatFOWCell *cell;

    numCells = fowCurrentGrid->nx * fowCurrentGrid->ny * fowCurrentGrid->nz;
    for (mode = 0; mode < 3; mode++)
    {
        fowBoundary[mode].faces = memAlloc(numCells * (sdword)sizeof(CatFOWFace), "fowFaces", 0);
        memset(fowBoundary[mode].faces, 0, numCells * sizeof(CatFOWFace));
    }
    // the exe initialises the lists in the order occupied, visited, empty, then zeroes the counts
    for (mode = 0; mode < 3; mode++)
    {
        listInit(&fowBoundary[mode].faceList);
        fowBoundary[mode].numDirty = 0;
    }
    for (mode = 0; mode < 3; mode++)
    {
        fowBoundary[mode].dirty = memAlloc(numCells * (sdword)sizeof(CatFOWDirty), "fowDirty", 0);
        memset(fowBoundary[mode].dirty, 0, numCells * sizeof(CatFOWDirty));
    }

    if (markAllDirty)
    {
        for (x = 0; x < fowCurrentGrid->nx; x++)
        {
            for (y = 0; y < fowCurrentGrid->ny; y++)
            {
                for (z = 0; z < fowCurrentGrid->nz; z++)
                {
                    cell = &fowCurrentGrid->cells[fowCellIndex(fowCurrentGrid, x, y, z)];
                    cell->dirty[0] = 0;
                    cell->dirty[1] = 0;
                    cell->dirty[2] = 0;
                    fowCellMarkDirty(x, y, z, FALSE);
                }
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_fowBoundaryDestroy
    Description : frees the face records and queues (the line batch stays)
----------------------------------------------------------------------------*/
static void cat_fowBoundaryDestroy(void)                    // 0x0047fb00
{
    sdword mode;

    for (mode = 0; mode < 3; mode++)
    {
        if (fowBoundary[mode].faces != NULL)
        {
            memFree(fowBoundary[mode].faces);
        }
        fowBoundary[mode].faces = NULL;
    }
    for (mode = 0; mode < 3; mode++)
    {
        if (fowBoundary[mode].dirty != NULL)
        {
            memFree(fowBoundary[mode].dirty);
        }
        fowBoundary[mode].dirty = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : fowCellMarkDirty
    Description : queues a cell (and with neighbours its six face neighbours) for each mode's rebuild
----------------------------------------------------------------------------*/
static void fowCellMarkDirty(sdword x, sdword y, sdword z, bool32 neighbours)  // 0x0047fb80
{
    CatFOWCell *cell;
    CatFOWBoundary *boundary;
    sdword mode;

    if (!fowInGrid(fowCurrentGrid, x, y, z))
    {
        return;
    }
    cell = &fowCurrentGrid->cells[fowCellIndex(fowCurrentGrid, x, y, z)];
    for (mode = 0; mode < 3; mode++)
    {
        if (cell->dirty[mode] == 0)
        {
            cell->dirty[mode] = 1;
            boundary = &fowBoundary[mode];
            boundary->dirty[boundary->numDirty].x = x;
            boundary->dirty[boundary->numDirty].y = y;
            boundary->dirty[boundary->numDirty].z = z;
            boundary->numDirty++;
        }
    }
    if (neighbours)
    {
        fowCellMarkDirty(x - 1, y, z, FALSE);
        fowCellMarkDirty(x + 1, y, z, FALSE);
        fowCellMarkDirty(x, y - 1, z, FALSE);
        fowCellMarkDirty(x, y + 1, z, FALSE);
        fowCellMarkDirty(x, y, z - 1, FALSE);
        fowCellMarkDirty(x, y, z + 1, FALSE);   // the exe's loop tail
    }
}

// is the cell in the mode's set: occupied (usage >= 1), visited, or visited and empty
static bool32 fowCellInSet(CatFOWCell *cell, sdword mode)
{
    switch (mode)
    {
        case FOW_BOUNDARY_OCCUPIED: return cell->usage >= 1;
        case FOW_BOUNDARY_VISITED:  return cell->visited != 0;
        default:                    return (cell->visited != 0) && (cell->usage < 1);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_fowBoundaryRebuildFaces
    Description : for each queued cell, works out which of its 12 edges lie on the boundary of the mode's set
                  and puts the cell's face record on (or takes it off) the mode's list
----------------------------------------------------------------------------*/
static void cat_fowBoundaryRebuildFaces(sdword mode)        // 0x0047fd80
{
    CatFOWBoundary *boundary = &fowBoundary[mode];
    CatFOWDirty *dirty;
    CatFOWCell *cell;
    CatFOWFace *face;
    sdword index, cellIndex, edge, n, nx, ny, nz;
    sdword neighbour[3];
    bool32 inSet[3], hide, occupied, anyEdge;

    for (index = 0; index < boundary->numDirty; index++)
    {
        dirty = &boundary->dirty[index];
        cellIndex = fowCellIndex(fowCurrentGrid, dirty->x, dirty->y, dirty->z);
        cell = &fowCurrentGrid->cells[cellIndex];
        face = &boundary->faces[cellIndex];

        if (!fowCellInSet(cell, mode))
        {
            memset(face->edge, 0, sizeof(face->edge));
            if (face->link.belongto != NULL)
            {
                listRemoveNode(&face->link);
            }
        }
        else
        {
            for (edge = 0; edge < 12; edge++)
            {
                for (n = 0; n < 3; n++)
                {
                    nx = dirty->x + fowEdgeNeighbours[edge][n][0];
                    ny = dirty->y + fowEdgeNeighbours[edge][n][1];
                    nz = dirty->z + fowEdgeNeighbours[edge][n][2];
                    neighbour[n] = fowInGrid(fowCurrentGrid, nx, ny, nz) ? fowCellIndex(fowCurrentGrid, nx, ny, nz) : -1;
                    inSet[n] = (neighbour[n] != -1) && fowCellInSet(&fowCurrentGrid->cells[neighbour[n]], mode);
                }
                // the edge is hidden inside a flat face (both sides in, the diagonal in) or (none in, the diagonal
                // in), or folded away (the first side in, the second and the diagonal out)
                if (inSet[0] || inSet[1])
                {
                    if (inSet[2] || inSet[1])
                    {
                        hide = (inSet[0] && inSet[1]) ? inSet[2] : FALSE;
                    }
                    else
                    {
                        hide = inSet[0];
                    }
                }
                else
                {
                    hide = inSet[2];
                }
                if (!hide && (mode == FOW_BOUNDARY_EMPTY))
                {
                    // an empty visited cell's edge next to an occupied cell is left to the occupied boundary
                    occupied = FALSE;
                    for (n = 0; n < 3; n++)
                    {
                        if ((neighbour[n] != -1) && (fowCurrentGrid->cells[neighbour[n]].usage > 0))
                        {
                            occupied = TRUE;
                        }
                    }
                    hide = occupied;
                }
                face->edge[edge] = hide ? 0 : 1;
            }

            if (face->link.belongto != NULL)
            {
                anyEdge = FALSE;
                for (edge = 0; edge < 12; edge++)
                {
                    if (face->edge[edge] == 1)
                    {
                        anyEdge = TRUE;
                    }
                }
                if (!anyEdge)
                {
                    listRemoveNode(&face->link);
                }
            }
            else
            {
                // CAT: as in the exe, a record goes on the list even with no edge drawn
                listAddNode(&boundary->faceList, &face->link, face);
                face->cell = cellIndex;
            }
        }
        cell->dirty[mode] = 0;
    }
    boundary->numDirty = 0;
}

/*-----------------------------------------------------------------------------
    Name        : cat_fowBoundaryUpdate
    Description : rebuilds the boundaries the sensors manager shows: the occupied one (with the empty visited
                  one) when catSmViewAreasOccupied, else the visited one, each only with catSmViewAreasVisited
----------------------------------------------------------------------------*/
static void cat_fowBoundaryUpdate(void)                     // 0x0047fcf0
{
    if (!catSmViewAreasOccupied)
    {
        if (catSmViewAreasVisited)
        {
            cat_fowBoundaryRebuildFaces(FOW_BOUNDARY_VISITED);
        }
    }
    else
    {
        cat_fowBoundaryRebuildFaces(FOW_BOUNDARY_OCCUPIED);
        if (catSmViewAreasVisited)
        {
            cat_fowBoundaryRebuildFaces(FOW_BOUNDARY_EMPTY);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_fowCellCornerGet
    Description : one of the 8 corners of a cell about its centre
----------------------------------------------------------------------------*/
static void cat_fowCellCornerGet(sdword corner, vector *centre, vector *out)  // 0x004801a0
{
    real32 h = fowCurrentGrid->cellSize * 0.5f;

    *out = *centre;
    switch (corner)
    {
        case 0: out->x -= h; out->y -= h; out->z += h; break;
        case 1: out->x += h; out->y -= h; out->z += h; break;
        case 2: out->x += h; out->y -= h; out->z -= h; break;
        case 3: out->x -= h; out->y -= h; out->z -= h; break;
        case 4: out->x -= h; out->y += h; out->z += h; break;
        case 5: out->x += h; out->y += h; out->z += h; break;
        case 6: out->x += h; out->y += h; out->z -= h; break;
        case 7: out->x -= h; out->y += h; out->z -= h; break;
        default: break;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_fowLineAdd
    Description : queues a line with its end points' distances from the sensors camera
----------------------------------------------------------------------------*/
static void cat_fowLineAdd(vector *p0, vector *p1)          // 0x00480a90
{
    CatFOWLine *line;
    vector diff;

    if (fowNumLines >= fowMaxLines)
    {
        fowMaxLines += 0x80;
        fowLines = memRealloc(fowLines, fowMaxLines * (sdword)sizeof(CatFOWLine), "fowLines", 0);
    }
    line = &fowLines[fowNumLines];
    line->p0 = *p0;
    line->p1 = *p1;
    vecSub(diff, smCamera.eyeposition, *p0);
    line->d0 = getVectDistSloppy(diff);
    vecSub(diff, smCamera.eyeposition, *p1);
    line->d1 = getVectDistSloppy(diff);
    fowNumLines++;
}

/*-----------------------------------------------------------------------------
    Name        : cat_fowCornerLinesAdd
    Description : the small triangle marking a corner where three boundary edges meet
----------------------------------------------------------------------------*/
static void cat_fowCornerLinesAdd(vector *corner, sdword cornerIndex)    // 0x00480be0
{
    vector a, b, c;

    vecAdd(a, *corner, fowCornerLines[cornerIndex][0]);
    vecAdd(b, *corner, fowCornerLines[cornerIndex][1]);
    vecAdd(c, *corner, fowCornerLines[cornerIndex][2]);
    cat_fowLineAdd(&a, &b);
    cat_fowLineAdd(&b, &c);
    cat_fowLineAdd(&c, &a);
}

static bool32 fowCornerAllEdges(CatFOWFace *face, sdword corner)
{
    return (face->edge[fowCornerEdges[corner][0]] != 0) && (face->edge[fowCornerEdges[corner][1]] != 0) &&
           (face->edge[fowCornerEdges[corner][2]] != 0);
}

/*-----------------------------------------------------------------------------
    Name        : cat_fowBoundaryDrawList
    Description : draws a mode's boundary edges as lines coloured by distance (16 steps between the nearest and
                  the farthest end point); at a corner where three edges meet the edges stop short and a corner
                  triangle is drawn instead
----------------------------------------------------------------------------*/
static void cat_fowBoundaryDrawList(LinkedList *faceList, color *colours)  // 0x00480650
{
    Node *node;
    CatFOWFace *face;
    vector centre, c0, c1, l0, l1, dir;
    sdword edge, corner0, corner1, index, colourIndex, lineIndex;
    real32 gap = FOW_CORNER_FRACTION * fowCurrentGrid->cellSize;
    real32 dmin, dmax, scale, f;
    color c;

    for (node = faceList->head; node != NULL; node = node->next)
    {
        face = (CatFOWFace *)node->structptr;
        centre = fowCurrentGrid->cells[face->cell].centre;
        for (edge = 0; edge < 12; edge++)
        {
            if (face->edge[edge] != 1)
            {
                continue;
            }
            corner0 = fowEdgeCorners[edge][0];
            corner1 = fowEdgeCorners[edge][1];
            cat_fowCellCornerGet(corner0, &centre, &c0);
            cat_fowCellCornerGet(corner1, &centre, &c1);
            l0 = c0;
            l1 = c1;
            if (fowCornerAllEdges(face, corner0))
            {
                vecSub(dir, c1, c0);
                vecNormalize(&dir);
                l0.x = gap * dir.x + c0.x;
                l0.y = gap * dir.y + c0.y;
                l0.z = gap * dir.z + c0.z;
                if (edge == fowCornerEdges[corner0][0])
                {
                    cat_fowCornerLinesAdd(&c0, corner0);
                }
            }
            if (fowCornerAllEdges(face, corner1))
            {
                vecSub(dir, c0, c1);
                vecNormalize(&dir);
                l1.x = gap * dir.x + c1.x;
                l1.y = gap * dir.y + c1.y;
                l1.z = gap * dir.z + c1.z;
                if (edge == fowCornerEdges[corner1][0])
                {
                    cat_fowCornerLinesAdd(&c1, corner1);
                }
            }
            cat_fowLineAdd(&l0, &l1);
        }
    }

    // the distance range (the exe starts from 1e21 / 0)
    dmin = 1.0e21f;
    dmax = 0.0f;
    for (index = 0; index < fowNumLines; index++)
    {
        if (fowLines[index].d0 < dmin) dmin = fowLines[index].d0; else if (dmax < fowLines[index].d0) dmax = fowLines[index].d0;
        if (fowLines[index].d1 < dmin) dmin = fowLines[index].d1; else if (dmax < fowLines[index].d1) dmax = fowLines[index].d1;
    }
    scale = (1.0f / (dmax - dmin)) * 16.0f;

    glBegin(GL_LINES);
    for (lineIndex = 0; lineIndex < fowNumLines; lineIndex++)
    {
        for (index = 0; index < 2; index++)
        {
            // CAT-FIX: the exe's __ftol of a NaN (one line, dmax == dmin) gives 0x80000000 and so colour 0; the
            //      float is clamped before the conversion here
            f = ((index == 0 ? fowLines[lineIndex].d0 : fowLines[lineIndex].d1) - dmin) * scale;
            if (!(f >= 0.0f))
            {
                colourIndex = 0;
            }
            else if (f >= 16.0f)
            {
                colourIndex = 15;
            }
            else
            {
                colourIndex = (sdword)f;
            }
            c = colours[colourIndex];
            glColor3ub(colRed(c), colGreen(c), colBlue(c));
            if (index == 0)
            {
                glVertex3f(fowLines[lineIndex].p0.x, fowLines[lineIndex].p0.y, fowLines[lineIndex].p0.z);
            }
            else
            {
                glVertex3f(fowLines[lineIndex].p1.x, fowLines[lineIndex].p1.y, fowLines[lineIndex].p1.z);
            }
        }
        // the exe restarts the batch every 64 lines, starting after the first
        if ((lineIndex & 0x3f) == 0)
        {
            glEnd();
            glBegin(GL_LINES);
        }
    }
    glEnd();
    fowNumLines = 0;
}

/*-----------------------------------------------------------------------------
    Name        : cat_fowBoundaryDraw
    Description : builds the colour ramps once and the corner triangles for the cell size, then draws the
                  boundaries cat_fowBoundaryUpdate rebuilt
----------------------------------------------------------------------------*/
static void cat_fowBoundaryDraw(void)                       // 0x004802d0
{
    sdword index;
    real32 factor, f;

    if (fowColoursDirty)
    {
        for (index = 0; index < 16; index++)
        {
            factor = (real32)index * 0.0625f;
            fowBoundary[FOW_BOUNDARY_OCCUPIED].colours[index] = colBlend(colRGB(fowOccupiedNear[0], fowOccupiedNear[1], fowOccupiedNear[2]),
                                                                         colRGB(fowOccupiedFar[0], fowOccupiedFar[1], fowOccupiedFar[2]), factor);
            fowBoundary[FOW_BOUNDARY_VISITED].colours[index] = colBlend(colRGB(fowVisitedNear[0], fowVisitedNear[1], fowVisitedNear[2]),
                                                                        colRGB(fowVisitedFar[0], fowVisitedFar[1], fowVisitedFar[2]), factor);
            fowBoundary[FOW_BOUNDARY_EMPTY].colours[index] = colBlend(colRGB(fowEmptyNear[0], fowEmptyNear[1], fowEmptyNear[2]),
                                                                      colRGB(fowEmptyFar[0], fowEmptyFar[1], fowEmptyFar[2]), factor);
        }
        fowColoursDirty = FALSE;
    }

    // the corner triangles: three points a fifth of a cell along the corner's edges, pointing into the cell
    // (0x00a3e328: per corner the y, x and z offsets)
    f = FOW_CORNER_FRACTION * fowCurrentGrid->cellSize;
    memset(fowCornerLines, 0, sizeof(fowCornerLines));
    fowCornerLines[0][0].y =  f; fowCornerLines[0][1].x =  f; fowCornerLines[0][2].z = -f;
    fowCornerLines[1][0].y =  f; fowCornerLines[1][1].x = -f; fowCornerLines[1][2].z = -f;
    fowCornerLines[2][0].y =  f; fowCornerLines[2][1].x = -f; fowCornerLines[2][2].z =  f;
    fowCornerLines[3][0].y =  f; fowCornerLines[3][1].x =  f; fowCornerLines[3][2].z =  f;
    fowCornerLines[4][0].y = -f; fowCornerLines[4][1].x =  f; fowCornerLines[4][2].z = -f;
    fowCornerLines[5][0].y = -f; fowCornerLines[5][1].x = -f; fowCornerLines[5][2].z = -f;
    fowCornerLines[6][0].y = -f; fowCornerLines[6][1].x = -f; fowCornerLines[6][2].z =  f;
    fowCornerLines[7][0].y = -f; fowCornerLines[7][1].x =  f; fowCornerLines[7][2].z =  f;

    if (!catSmViewAreasOccupied)
    {
        if (catSmViewAreasVisited)
        {
            cat_fowBoundaryDrawList(&fowBoundary[FOW_BOUNDARY_VISITED].faceList, fowBoundary[FOW_BOUNDARY_VISITED].colours);
        }
    }
    else
    {
        cat_fowBoundaryDrawList(&fowBoundary[FOW_BOUNDARY_OCCUPIED].faceList, fowBoundary[FOW_BOUNDARY_OCCUPIED].colours);
        if (catSmViewAreasVisited)
        {
            cat_fowBoundaryDrawList(&fowBoundary[FOW_BOUNDARY_EMPTY].faceList, fowBoundary[FOW_BOUNDARY_EMPTY].colours);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : fowRender
    Description : draws the selected grid's boundaries (depth test off)
----------------------------------------------------------------------------*/
void fowRender(void)                                        // 0x0047f810
{
    GLboolean depthTest;

    if (fowCurrentGrid == NULL)
    {
        return;     // CAT-FIX: the exe's callers only render a selected grid
    }
    depthTest = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    cat_fowBoundaryUpdate();
    cat_fowBoundaryDraw();
    if (depthTest)
    {
        glEnable(GL_DEPTH_TEST);
    }
}

/*=============================================================================
    Visibility:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : cat_fowCanPlayerSeeShip
    Description : can the player see the ship: with no grid (an AI player) a ship is seen unless fully cloaked
                  and undetected; with a grid it must also be in an occupied cell (or outside the grid), and a
                  hidden ship or one outside the universe is not seen. In multiplayer, multiplayerSeesAll
                  short-cuts to TRUE. NB: selects the player's grid and leaves it selected, as the exe does.
----------------------------------------------------------------------------*/
bool32 cat_fowCanPlayerSeeShip(Player *player, Ship *ship, bool32 multiplayerSeesAll)  // 0x0047e810
{
    CatFOWGrid *grid;
    sdword usage;
    bool32 seen = FALSE;

    if (!singlePlayerGame && multiplayerSeesAll)
    {
        return TRUE;
    }
    grid = (CatFOWGrid *)player->catFOWGrid;
    if (grid == NULL)
    {
        if ((player->aiPlayer == NULL) || !cat_shipIsCloakedFully(ship) ||
            cat_shipDetectedByPlayer(ship, player->playerIndex))
        {
            return TRUE;
        }
        return FALSE;
    }

    fowCurrentGrid = grid;
    usage = fowGetUsageCount(fowToCell(ship->obj.posinfo.position.x, grid->origin.x, grid->cellSize),
                             fowToCell(ship->obj.posinfo.position.y, grid->origin.y, grid->cellSize),
                             fowToCell(ship->obj.posinfo.position.z, grid->origin.z, grid->cellSize));
    if ((usage > 0) && (!cat_shipIsCloakedFully(ship) || cat_shipDetectedByPlayer(ship, player->playerIndex)))
    {
        seen = TRUE;
    }
    if (usage == FOW_OUTSIDE_GRID)
    {
        seen = TRUE;
    }
    if ((ship != NULL) && (ship->header.objtype == OBJ_ShipType))
    {
        if (bitTest(ship->header.flags, SOF_Hide))
        {
            seen = FALSE;
        }
        if (ship->shiplink.belongto == NULL)
        {
            return FALSE;
        }
    }
    return seen;
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipAffectsFOW
    Return      : TRUE unless the ship is a component or a swarmer
----------------------------------------------------------------------------*/
bool32 cat_shipAffectsFOW(Ship *ship)                       // 0x00480ce0
{
    return (ship->staticinfo->shipclass != CLASS_Component) &&
           (ship->shiptype != sSwarmer) && (ship->shiptype != bSwarmer);
}
