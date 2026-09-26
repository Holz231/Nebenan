// SPDX-License-Identifier: MIT

#pragma once

#include "poly.h"

#include "box3d/collision.h"

// A face shared by two Voronoi cells
typedef struct nbCellNeighbor
{
	// Index of the neighboring site
	int site;

	// The shared face, normal pointing to the neighbor
	nbBondGeometry geometry;
} nbCellNeighbor;

typedef struct nbCell
{
	// Heap allocated shape in the frame of the job origin, owned by the caller. Null if the cell was
	// empty or too small.
	nbShape* shape;

	// Box3D hull of the shape in arena memory, if the job builds hulls. Null for a sliver without a
	// valid hull, which is dropped.
	b3HullData* hull;

	// Faces shared with other cells, in the frame of the sites and in arena memory. They become the
	// internal bonds.
	nbCellNeighbor* neighbors;
	int neighborCount;
} nbCell;

// Split a convex parent into the Voronoi cells of the given sites. Cells are clipped to the parent,
// so they tile it exactly. Every cell is a pure function of the job, so the cells can be computed in
// any order on any thread and the result is always bit for bit the same.
typedef struct nbFractureJob
{
	// Parent and sites in a frame centered on the parent, for precision
	const nbPoly* parent;
	const b3Vec3* sites;
	int siteCount;

	// Added to the finished cell shapes, which moves them back to the frame of the parent chunk
	b3Vec3 origin;

	uint8_t interiorMaterial;
	float tolerance;
	float minVolume;

	// Build the Box3D hull of every cell as well
	bool buildHulls;

	// Output, one cell per site
	nbCell* cells;
} nbFractureJob;

// Counters of one worker
typedef struct nbFractureCounters
{
	int clipCount;

	// Clip operations that failed numerically. The affected plane is skipped.
	int failureCount;

	// Hulls that needed quickhull instead of the direct build
	int hullFallbackCount;
} nbFractureCounters;

// Scratch memory of one worker for jobs with up to siteCapacity sites
typedef struct nbCellScratch
{
	nbPoly* polyA;
	nbPoly* polyB;
	uint64_t* heap;
	int siteCapacity;
} nbCellScratch;

void nbCellScratch_Create( nbCellScratch* scratch, nbArena* arena, int siteCapacity );

// Compute one cell of a job. The neighbors and the hull go to the arena, the shape to the heap.
void nbComputeCell( const nbFractureJob* job, int cellIndex, nbArena* arena, nbCellScratch* scratch, nbFractureCounters* counters );

typedef struct nbFractureOutput
{
	// One cell per site
	nbCell* cells;
	int cellCount;

	int failureCount;
	int clipCount;
} nbFractureOutput;

// Compute all cells on the calling thread, without hulls and with the cells left in the frame of the
// sites. For tests and benchmarks, the world runs fracture jobs with nbRunFractureJobs.
void nbComputeVoronoiCells( nbArena* arena, const nbPoly* parent, const b3Vec3* sites, int siteCount, uint8_t interiorMaterial,
							float tolerance, float minVolume, nbFractureOutput* output );

typedef struct nbSiteParams
{
	// Impact point in the frame of the parent
	b3Vec3 center;

	// Damage radius
	float radius;

	// Sites inside the damage radius. Denser toward the center.
	int innerCount;

	// Sites on a shell around the damage radius. They shape the rim of the hole.
	int ringCount;

	// Sites spread over the rest of the parent. They cut far regions into large pieces.
	int outerCount;

	// Minimum distance between sites. Avoids slivers.
	float minSpacing;
} nbSiteParams;

// Generate fracture sites inside a convex parent. Deterministic for a given random state.
int nbGenerateSites( const nbPoly* parent, const nbSiteParams* params, nbRandom* rng, b3Vec3* sites, int capacity );

// Estimate the volume of the intersection of the parent and a sphere by sampling.
float nbEstimateSphereOverlap( const nbPoly* parent, b3Vec3 center, float radius, nbRandom* rng, int sampleCount );

// Uniformly distributed random unit vector without trigonometry, for determinism.
b3Vec3 nbRandomUnitVector( nbRandom* rng );
