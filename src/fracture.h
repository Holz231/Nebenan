// SPDX-License-Identifier: MIT

#pragma once

#include "poly.h"

// A face shared by two Voronoi cells
typedef struct nbCellNeighbor
{
	// Index of the neighboring site
	int site;
	float area;
	b3Vec3 centroid;
} nbCellNeighbor;

typedef struct nbCell
{
	// Heap allocated shape, owned by the caller. Null if the cell was empty or too small.
	nbShape* shape;
	int firstNeighbor;
	int neighborCount;
} nbCell;

typedef struct nbFractureOutput
{
	// One cell per site
	nbCell* cells;
	nbCellNeighbor* neighbors;
	int cellCount;
	int neighborCount;

	// Clip operations that failed numerically. The affected plane is skipped.
	int failureCount;
	int clipCount;
} nbFractureOutput;

// Split a convex parent into the Voronoi cells of the given sites. Cells are clipped to the parent,
// so they tile it exactly. Scratch memory comes from the arena, cell shapes are heap allocated.
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
