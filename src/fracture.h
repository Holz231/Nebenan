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

// A convex part of a cell that cutouts cut apart
typedef struct nbCellPart
{
	nbShape* shape;
} nbCellPart;

typedef struct nbCell
{
	// Heap allocated shape in the frame of the job origin, owned by the caller. Null if the cell was
	// empty or too small, or if cutouts cut it into parts. If the job builds hulls, the shape carries
	// its Box3D hull, or none for a sliver without a valid hull, which is dropped.
	nbShape* shape;

	// Faces shared with other cells, in the frame of the sites and in arena memory. They become the
	// internal bonds. A job with cutouts leaves them out.
	nbCellNeighbor* neighbors;
	int neighborCount;

	// What cutouts left of the cell, in arena memory with the shapes on the heap like the shape
	nbCellPart* parts;
	int partCount;
} nbCell;

// A box cut out of the parent of a job, a window or a door: the points behind all six planes, in the
// frame of the sites
typedef struct nbCutout
{
	b3Plane planes[6];
	b3AABB bounds;
} nbCutout;

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

	// Compute the second moments of the faces between cells, see nbBondGeometry
	bool bondMoments;

	// Boxes cut out of every cell, and the material of their faces. A cell they cut falls apart into
	// convex parts. Parts thinner than the minimum width are slivers where a cell reaches just around a
	// corner of a cutout, they are dropped.
	const nbCutout* cutouts;
	int cutoutCount;
	uint8_t cutoutMaterial;
	float cutoutMinWidth;

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

	// Room for the parts of a cell with cutouts, taken from the arena at the first one
	nbPoly* partPolys;
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
