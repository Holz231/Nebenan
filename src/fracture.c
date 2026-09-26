// SPDX-License-Identifier: MIT

#include "fracture.h"

#include "hull_builder.h"

#include <float.h>

// Min-heap of (squared distance, site) keys. The key packs the float bits of the squared distance
// above the site index. Positive floats sort like their bit patterns, so integer compares give a
// deterministic order with ties broken by index.
static void nbHeap_SiftDown( uint64_t* heap, int count, int index )
{
	uint64_t key = heap[index];
	for ( ;; )
	{
		int child = 2 * index + 1;
		if ( child >= count )
		{
			break;
		}

		if ( child + 1 < count && heap[child + 1] < heap[child] )
		{
			child += 1;
		}

		if ( key <= heap[child] )
		{
			break;
		}

		heap[index] = heap[child];
		index = child;
	}
	heap[index] = key;
}

static uint64_t nbHeap_Pop( uint64_t* heap, int* count )
{
	uint64_t top = heap[0];
	*count -= 1;
	if ( *count > 0 )
	{
		heap[0] = heap[*count];
		nbHeap_SiftDown( heap, *count, 0 );
	}
	return top;
}

static uint32_t nbFloatBits( float value )
{
	uint32_t bits;
	memcpy( &bits, &value, sizeof( bits ) );
	return bits;
}

static float nbBitsToFloat( uint32_t bits )
{
	float value;
	memcpy( &value, &bits, sizeof( value ) );
	return value;
}

void nbCellScratch_Create( nbCellScratch* scratch, nbArena* arena, int siteCapacity )
{
	scratch->polyA = nbArena_AllocArray( arena, nbPoly, 1 );
	scratch->polyB = nbArena_AllocArray( arena, nbPoly, 1 );
	scratch->heap = nbArena_AllocArray( arena, uint64_t, siteCapacity > 0 ? siteCapacity : 1 );
	scratch->siteCapacity = siteCapacity;
}

void nbComputeCell( const nbFractureJob* job, int cellIndex, nbArena* arena, nbCellScratch* scratch, nbFractureCounters* counters )
{
	NB_ASSERT( job->siteCount <= scratch->siteCapacity );

	nbCell* cell = job->cells + cellIndex;
	cell->shape = NULL;
	cell->hull = NULL;
	cell->neighbors = NULL;
	cell->neighborCount = 0;

	const b3Vec3* sites = job->sites;
	int siteCount = job->siteCount;
	b3Vec3 site = sites[cellIndex];
	uint64_t* heap = scratch->heap;

	int heapCount = 0;
	for ( int j = 0; j < siteCount; ++j )
	{
		if ( j == cellIndex )
		{
			continue;
		}
		float distanceSquared = b3DistanceSquared( site, sites[j] );
		heap[heapCount++] = ( (uint64_t)nbFloatBits( distanceSquared ) << 32 ) | (uint64_t)j;
	}

	for ( int k = heapCount / 2 - 1; k >= 0; --k )
	{
		nbHeap_SiftDown( heap, heapCount, k );
	}

	nbPoly* current = scratch->polyA;
	nbPoly* next = scratch->polyB;
	*current = *job->parent;

	float maxRadiusSquared = nbPoly_MaxDistanceSquared( current, site );

	while ( heapCount > 0 )
	{
		uint64_t key = nbHeap_Pop( heap, &heapCount );
		float distanceSquared = nbBitsToFloat( (uint32_t)( key >> 32 ) );
		int j = (int)( key & 0xFFFFFFFFu );

		// The bisector is half the distance away. Once it is beyond the farthest vertex of the
		// cell no remaining site can cut the cell.
		if ( distanceSquared > 4.0f * maxRadiusSquared )
		{
			break;
		}

		b3Vec3 other = sites[j];
		b3Vec3 normal = b3Normalize( b3Sub( other, site ) );
		b3Vec3 midPoint = b3MulSV( 0.5f, b3Add( site, other ) );
		b3Plane plane = { normal, b3Dot( normal, midPoint ) };

		counters->clipCount += 1;
		nbClipResult result = nbPoly_Clip( current, plane, job->interiorMaterial, j, job->tolerance, next );
		if ( result == nb_clipCut )
		{
			nbPoly* swap = current;
			current = next;
			next = swap;
			maxRadiusSquared = nbPoly_MaxDistanceSquared( current, site );
		}
		else if ( result == nb_clipEmpty )
		{
			// Happens for sites outside the parent or coincident sites
			return;
		}
		else if ( result == nb_clipOverflow )
		{
			counters->failureCount += 1;
		}
	}

	float volume;
	b3Vec3 centroid;
	nbPoly_ComputeMass( current, &volume, &centroid );
	if ( volume < job->minVolume )
	{
		return;
	}

	nbShape* shape = nbShape_CreateWithMass( current, volume, centroid );
	if ( shape == NULL )
	{
		return;
	}

	// Record the faces shared with other cells. These become the internal bonds.
	int taggedCount = 0;
	for ( int f = 0; f < current->faceCount; ++f )
	{
		taggedCount += current->faces[f].tag >= 0 ? 1 : 0;
	}

	if ( taggedCount > 0 )
	{
		cell->neighbors = nbArena_AllocArray( arena, nbCellNeighbor, taggedCount );
	}

	for ( int f = 0; f < current->faceCount; ++f )
	{
		int tag = current->faces[f].tag;
		if ( tag < 0 )
		{
			continue;
		}

		b3Vec3 faceCentroid;
		float area = nbPoly_FaceArea( current, f, &faceCentroid );
		if ( area <= 0.0f )
		{
			continue;
		}

		nbCellNeighbor* neighbor = cell->neighbors + cell->neighborCount;
		neighbor->site = tag;
		neighbor->area = area;
		neighbor->centroid = faceCentroid;
		cell->neighborCount += 1;
	}

	nbShape_Translate( shape, job->origin );
	cell->shape = shape;

	if ( job->buildHulls )
	{
		cell->hull = nbCreateHullInArena( shape, arena, &counters->hullFallbackCount );
	}
}

void nbComputeVoronoiCells( nbArena* arena, const nbPoly* parent, const b3Vec3* sites, int siteCount, uint8_t interiorMaterial,
							float tolerance, float minVolume, nbFractureOutput* output )
{
	nbFractureJob job = {
		.parent = parent,
		.sites = sites,
		.siteCount = siteCount,
		.origin = b3Vec3_zero,
		.interiorMaterial = interiorMaterial,
		.tolerance = tolerance,
		.minVolume = minVolume,
		.buildHulls = false,
		.cells = nbArena_AllocArray( arena, nbCell, siteCount ),
	};

	nbCellScratch scratch;
	nbCellScratch_Create( &scratch, arena, siteCount );

	nbFractureCounters counters = { 0 };
	for ( int i = 0; i < siteCount; ++i )
	{
		nbComputeCell( &job, i, arena, &scratch, &counters );
	}

	output->cells = job.cells;
	output->cellCount = siteCount;
	output->failureCount = counters.failureCount;
	output->clipCount = counters.clipCount;
}

// Uniform point in the cube [-1, 1]^3. One draw per statement: C leaves the evaluation order inside an
// initializer or an argument list open, and compilers differ, but fracture patterns must not.
static b3Vec3 nbRandomInCube( nbRandom* rng )
{
	float x = 2.0f * nbRandomFloat( rng ) - 1.0f;
	float y = 2.0f * nbRandomFloat( rng ) - 1.0f;
	float z = 2.0f * nbRandomFloat( rng ) - 1.0f;
	return (b3Vec3){ x, y, z };
}

b3Vec3 nbRandomUnitVector( nbRandom* rng )
{
	for ( int i = 0; i < 32; ++i )
	{
		b3Vec3 v = nbRandomInCube( rng );
		float lengthSquared = b3LengthSquared( v );
		if ( 1.0e-4f < lengthSquared && lengthSquared <= 1.0f )
		{
			return b3MulSV( 1.0f / sqrtf( lengthSquared ), v );
		}
	}
	return b3Vec3_axisY;
}

static bool nbIsTooClose( b3Vec3 point, const b3Vec3* sites, int first, int count, float minSpacingSquared )
{
	for ( int i = first; i < count; ++i )
	{
		if ( b3DistanceSquared( point, sites[i] ) < minSpacingSquared )
		{
			return true;
		}
	}
	return false;
}

// Spatial hash for the spacing test of the dense inner sites. Cells are one spacing wide, so a
// candidate only needs to look at the 27 surrounding cells.
#define NB_SITE_GRID_SIZE 1024
#define NB_SITE_CAPACITY 512

typedef struct nbSiteGrid
{
	uint32_t keys[NB_SITE_GRID_SIZE];
	int16_t heads[NB_SITE_GRID_SIZE];
	int16_t next[NB_SITE_CAPACITY];
	float inverseCellSize;
} nbSiteGrid;

static uint32_t nbGridKey( int x, int y, int z )
{
	// 10 bits per axis, offset so negative cells are positive. Key zero is reserved for empty.
	uint32_t ux = (uint32_t)( x + 512 ) & 1023u;
	uint32_t uy = (uint32_t)( y + 512 ) & 1023u;
	uint32_t uz = (uint32_t)( z + 512 ) & 1023u;
	return ( ( ux << 20 ) | ( uy << 10 ) | uz ) + 1u;
}

static int nbGridSlot( const nbSiteGrid* grid, uint32_t key )
{
	uint32_t slot = ( key * 2654435761u ) & ( NB_SITE_GRID_SIZE - 1 );
	while ( grid->keys[slot] != 0 && grid->keys[slot] != key )
	{
		slot = ( slot + 1 ) & ( NB_SITE_GRID_SIZE - 1 );
	}
	return (int)slot;
}

static void nbGridCell( const nbSiteGrid* grid, b3Vec3 p, int* x, int* y, int* z )
{
	*x = (int)floorf( p.x * grid->inverseCellSize );
	*y = (int)floorf( p.y * grid->inverseCellSize );
	*z = (int)floorf( p.z * grid->inverseCellSize );
}

static void nbGridInsert( nbSiteGrid* grid, const b3Vec3* sites, int index )
{
	int x, y, z;
	nbGridCell( grid, sites[index], &x, &y, &z );
	uint32_t key = nbGridKey( x, y, z );
	int slot = nbGridSlot( grid, key );
	if ( grid->keys[slot] == 0 )
	{
		grid->keys[slot] = key;
		grid->heads[slot] = -1;
	}
	grid->next[index] = grid->heads[slot];
	grid->heads[slot] = (int16_t)index;
}

static bool nbGridIsTooClose( const nbSiteGrid* grid, const b3Vec3* sites, b3Vec3 p, float spacingSquared )
{
	int x, y, z;
	nbGridCell( grid, p, &x, &y, &z );
	for ( int i = -1; i <= 1; ++i )
	{
		for ( int j = -1; j <= 1; ++j )
		{
			for ( int k = -1; k <= 1; ++k )
			{
				uint32_t key = nbGridKey( x + i, y + j, z + k );
				int slot = nbGridSlot( grid, key );
				if ( grid->keys[slot] == 0 )
				{
					continue;
				}

				for ( int s = grid->heads[slot]; s >= 0; s = grid->next[s] )
				{
					if ( b3DistanceSquared( p, sites[s] ) < spacingSquared )
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

static b3Vec3 nbRandomPointInBox( nbRandom* rng, b3AABB box )
{
	b3Vec3 extent = b3Sub( box.upperBound, box.lowerBound );
	float x = nbRandomFloat( rng );
	float y = nbRandomFloat( rng );
	float z = nbRandomFloat( rng );
	return (b3Vec3){
		box.lowerBound.x + extent.x * x,
		box.lowerBound.y + extent.y * y,
		box.lowerBound.z + extent.z * z,
	};
}

static b3AABB nbIntersectBoxes( b3AABB a, b3AABB b )
{
	b3AABB box = { b3Max( a.lowerBound, b.lowerBound ), b3Min( a.upperBound, b.upperBound ) };
	return box;
}

static bool nbIsEmptyBox( b3AABB box )
{
	return box.lowerBound.x > box.upperBound.x || box.lowerBound.y > box.upperBound.y || box.lowerBound.z > box.upperBound.z;
}

int nbGenerateSites( const nbPoly* parent, const nbSiteParams* params, nbRandom* rng, b3Vec3* sites, int capacity )
{
	capacity = capacity < NB_SITE_CAPACITY ? capacity : NB_SITE_CAPACITY;

	int count = 0;
	float spacing = params->minSpacing;
	float spacingSquared = spacing * spacing;
	float margin = 0.25f * spacing;
	float radius = params->radius;

	// Candidates are drawn from the part of the parent's bounds near the impact. This keeps the
	// acceptance rate high for thin walls and for impact points in front of a surface. The fragment
	// density falls off with the distance to the focus, the nearest point of the bounds.
	b3AABB bounds = nbPoly_ComputeBounds( parent );
	b3Vec3 focus = b3Clamp( params->center, bounds.lowerBound, bounds.upperBound );
	float minRadius = b3MaxFloat( 0.12f * radius, spacing );

	nbSiteGrid grid;
	memset( grid.keys, 0, sizeof( grid.keys ) );
	grid.inverseCellSize = 1.0f / spacing;

	int innerTarget = params->innerCount < capacity ? params->innerCount : capacity;
	b3AABB innerBox = nbIntersectBoxes( bounds, (b3AABB){ b3Sub( focus, (b3Vec3){ radius, radius, radius } ),
														   b3Add( focus, (b3Vec3){ radius, radius, radius } ) } );
	if ( nbIsEmptyBox( innerBox ) == false )
	{
		int maxAttempts = 24 * innerTarget + 64;
		for ( int attempt = 0; attempt < maxAttempts && count < innerTarget; ++attempt )
		{
			// Give up early when the sphere barely touches the parent
			if ( attempt == 256 && count == 0 )
			{
				break;
			}

			b3Vec3 p = nbRandomPointInBox( rng, innerBox );
			float distance = b3Distance( p, focus );
			if ( distance > radius )
			{
				continue;
			}

			// Density proportional to 1 / distance: small fragments at the focus, larger toward the rim
			if ( distance > minRadius && nbRandomFloat( rng ) * distance > minRadius )
			{
				continue;
			}

			if ( nbPoly_ContainsPoint( parent, p, margin ) == false || nbGridIsTooClose( &grid, sites, p, spacingSquared ) )
			{
				continue;
			}

			sites[count] = p;
			nbGridInsert( &grid, sites, count );
			count += 1;
		}
	}

	// Ring sites just outside the damage radius shape the rim of the hole
	int ringFirst = count;
	int ringTarget = count + params->ringCount;
	ringTarget = ringTarget < capacity ? ringTarget : capacity;
	float ringSpacingSquared = 4.0f * spacingSquared;
	float ringRadius = 1.6f * radius;
	b3AABB ringBox = nbIntersectBoxes( bounds, (b3AABB){ b3Sub( focus, (b3Vec3){ ringRadius, ringRadius, ringRadius } ),
														  b3Add( focus, (b3Vec3){ ringRadius, ringRadius, ringRadius } ) } );
	if ( nbIsEmptyBox( ringBox ) == false )
	{
		int maxAttempts = 24 * params->ringCount + 64;
		for ( int attempt = 0; attempt < maxAttempts && count < ringTarget; ++attempt )
		{
			b3Vec3 p = nbRandomPointInBox( rng, ringBox );
			float distance = b3Distance( p, focus );
			if ( distance < radius || distance > ringRadius )
			{
				continue;
			}

			if ( nbPoly_ContainsPoint( parent, p, margin ) == false || nbGridIsTooClose( &grid, sites, p, spacingSquared ) ||
				 nbIsTooClose( p, sites, ringFirst, count, ringSpacingSquared ) )
			{
				continue;
			}

			sites[count] = p;
			nbGridInsert( &grid, sites, count );
			count += 1;
		}
	}

	// Outer sites anywhere in the parent away from the impact. They are far from the inner sites
	// by construction, so they only need spacing among the ring and outer sites.
	int outerTarget = count + params->outerCount;
	outerTarget = outerTarget < capacity ? outerTarget : capacity;
	if ( params->outerCount > 0 )
	{
		b3Vec3 extent = b3Sub( bounds.upperBound, bounds.lowerBound );
		float exclusionSquared = ringRadius * ringRadius;

		// Spread the outer sites apart so the far pieces have similar sizes
		float volume = extent.x * extent.y * extent.z;
		float outerSpacing = 0.5f * nbCbrt( volume / (float)( params->outerCount + 1 ) );
		float largest = b3MaxFloat( extent.x, b3MaxFloat( extent.y, extent.z ) );
		outerSpacing = b3MinFloat( outerSpacing, 0.25f * largest );
		float outerSpacingSquared = b3MaxFloat( outerSpacing * outerSpacing, ringSpacingSquared );

		int maxAttempts = 24 * params->outerCount + 64;
		for ( int attempt = 0; attempt < maxAttempts && count < outerTarget; ++attempt )
		{
			b3Vec3 p = nbRandomPointInBox( rng, bounds );
			if ( b3DistanceSquared( p, focus ) < exclusionSquared || nbPoly_ContainsPoint( parent, p, margin ) == false ||
				 nbIsTooClose( p, sites, ringFirst, count, outerSpacingSquared ) )
			{
				continue;
			}

			sites[count++] = p;
		}
	}

	return count;
}

float nbEstimateSphereOverlap( const nbPoly* parent, b3Vec3 center, float radius, nbRandom* rng, int sampleCount )
{
	int inside = 0;
	int total = 0;
	for ( int i = 0; i < sampleCount; ++i )
	{
		b3Vec3 p = nbRandomInCube( rng );
		if ( b3LengthSquared( p ) > 1.0f )
		{
			continue;
		}
		total += 1;
		inside += nbPoly_ContainsPoint( parent, b3MulAdd( center, radius, p ), 0.0f ) ? 1 : 0;
	}

	if ( total == 0 )
	{
		return 0.0f;
	}

	float sphereVolume = ( 4.0f / 3.0f ) * B3_PI * radius * radius * radius;
	return sphereVolume * (float)inside / (float)total;
}
