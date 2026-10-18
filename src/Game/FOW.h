// =============================================================================
//  FOW.h
//  - Cataclysm's fog of war: a coarse 3D grid of cells each player's ships
//    light up, the explored/occupied boundary drawn in the sensors manager,
//    and the visibility tests (cataclysm.exe 0x0047e7f0-0x00480d50)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___FOW_H
#define ___FOW_H

#include <stddef.h>

#include "SpaceObj.h"
#include "Types.h"
#include "Vector.h"

struct Player;

// CAT: one cell of the grid (0x20 bytes; fowCreate 0x0047e9a0 fills the centres)
typedef struct CatFOWCell
{
    sdword usage;           // +0x00 how many cubes of the player's ships cover the cell now
    sdword visited;         // +0x04 TRUE once any cube covered it
    vector centre;          // +0x08 origin + (i + 0.5) * cellSize
    sdword dirty[3];        // +0x14 queued for the boundary rebuild: [0] occupied (mode 2), [1] visited (mode 1),
                            //       [2] visited and empty (mode 0) (fowCellMarkDirty 0x0047fb80)
} CatFOWCell;
_Static_assert(sizeof(CatFOWCell) == 0x20, "a FOW cell is 0x20 bytes in cataclysm.exe");

// CAT: a grid (0x20 bytes in the exe; Player.catFOWGrid points at one)
typedef struct CatFOWGrid
{
    CatFOWCell *cells;      // +0x00 nx * ny * nz cells, x fastest
    sdword nx, ny, nz;      // +0x04
    vector origin;          // +0x10 the box's low corner
    real32 cellSize;        // +0x1C 10000.0 (0x0087f774)
} CatFOWGrid;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatFOWGrid) == 0x20, "the FOW grid is 0x20 bytes in cataclysm.exe");
_Static_assert(offsetof(CatFOWGrid, cellSize) == 0x1C, "CatFOWGrid.cellSize at 0x1C");
#endif

// the usage/visited queries return this outside the grid
#define FOW_OUTSIDE_GRID        (-0x80000)

#define FOW_DEFAULT_CELL_SIZE   10000.0f        // 0x0087f774

void *fowCreate(vector *boxMin, vector *boxMax);                    // 0x0047e9a0
void fowDestroy(void *grid);                                        // 0x0047ed30
void *fowSelect(void *grid);                                        // 0x0047ed70
void fowClear(void);                                                // 0x0047ed80
void fowActivate(sdword x, sdword y, sdword z);                     // 0x0047edf0
void fowActivateCube(sdword x, sdword y, sdword z, sdword r);       // 0x0047eeb0
void fowActivateCubeW(vector pos, real32 radius);                   // 0x0047efb0
void fowDeactivateCube(sdword x, sdword y, sdword z, sdword r);     // 0x0047f070
void fowDeactivateCubeW(vector pos, real32 radius);                 // 0x0047f1d0
sdword fowGetUsageCount(sdword x, sdword y, sdword z);              // 0x0047f290
sdword fowGetUsageCountW(vector pos);                               // 0x0047f300
bool32 fowCubeHasUnvisited(sdword x, sdword y, sdword z, sdword r); // 0x0047f400
sdword fowGetVisited(sdword x, sdword y, sdword z);                 // 0x0047f540
sdword fowGetVisitedW(vector pos);                                  // 0x0047f5b0
void fowWorldToGrid(real32 x, real32 y, real32 z, real32 r, sdword *gx, sdword *gy, sdword *gz, sdword *gr);  // 0x0047f6b0
bool32 fowIsValidW(real32 x, real32 y, real32 z);                   // 0x0047f750
void fowRender(void);                                               // 0x0047f810
void cat_fowBoundaryCreate(bool32 markAllDirty);                    // 0x0047f850

sdword cat_fowGridSize(sdword nx, sdword ny, sdword nz);            // 0x0047e7f0 (bytes of the cells)

bool32 cat_fowCanPlayerSeeShip(struct Player *player, Ship *ship, bool32 multiplayerSeesAll);   // 0x0047e810
bool32 cat_shipAffectsFOW(Ship *ship);                              // 0x00480ce0

// CAT: the players' grids (LevelLoad.c: cat_fowCreatePlayerGrids 0x004a38c0, at levelInit and singlePlayerLoadNewLevel)
bool32 cat_fowCreatePlayerGrids(void);

// CAT: a ship's cube on its player's grid (UnivUpdate.c: 0x00542240, 0x005422b0, 0x00544b10)
void cat_univShipFOWActivate(Ship *ship);
void cat_univShipFOWDeactivate(Ship *ship);
void cat_univShipFOWUpdate(Ship *ship, vector *oldPos, vector *newPos);

#endif
