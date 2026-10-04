// SPDX-License-Identifier: MIT

#include "fracture.h"
#include "hull_builder.h"
#include "world.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

nbMaterial nbDefaultMaterial( void )
{
	nbMaterial material = { 0 };
	material.density = 2400.0f;
	material.friction = 0.7f;
	material.restitution = 0.05f;
	material.strength = 1.0e6f;
	material.fragmentSize = 0.12f;
	material.minFragmentVolume = 2.0e-6f;
	material.maxDepth = 6;
	material.userMaterialId = 0;
	return material;
}

nbDestructibleDef nbDefaultDestructibleDef( void )
{
	nbDestructibleDef def = { 0 };
	def.position = b3ToPos( b3Vec3_zero );
	def.rotation = b3Quat_identity;
	def.material = nbDefaultMaterial();
	def.isStatic = true;
	def.anchorCount = 0;
	def.cellSize = 0.0f;
	def.seed = 1;
	def.filter = b3DefaultFilter();
	def.enableCollisionDamage = true;
	def.internalValue = NB_SECRET_COOKIE;
	return def;
}

nbPieceDef nbDefaultPieceDef( void )
{
	nbPieceDef def = { 0 };
	def.halfExtents = (b3Vec3){ 0.5f, 0.5f, 0.5f };
	def.transform = b3Transform_identity;
	def.surfaceMaterial = 0;
	def.interiorMaterial = 1;
	return def;
}

nbDestructible* nbGetDestructibleFromId( nbDestructibleId id, nbWorld** worldOut )
{
	if ( id.index1 < 1 || id.world0 >= NB_MAX_WORLDS )
	{
		return NULL;
	}

	nbWorld* world = nbGetWorld( id.world0 );
	if ( world->inUse == false || id.index1 > world->destructibles.count )
	{
		return NULL;
	}

	nbDestructible* destructible = world->destructibles.data + ( id.index1 - 1 );
	if ( destructible->isFree || destructible->generation != id.generation )
	{
		return NULL;
	}

	if ( worldOut != NULL )
	{
		*worldOut = world;
	}
	return destructible;
}

// Where a cell reaches just around the corner of an opening, the part of it beyond is dropped if it is thinner than this
// part of the cell size. It would be a sliver, too thin to carry anything and a pain to simulate.
#define NB_SLIVER_WIDTH 0.05f

// Cells run across the seams between pieces of the same material and cell size, so a building cracks like one mass and
// not along the faces where its pieces meet, like the corners of a house. Every piece takes the sites of the other pieces
// within this many cell sizes of it into its own Voronoi diagram. Sites farther away own no part of it.
#define NB_SEAM_REACH 2.0f

// Sites of one piece at most, its own and those it takes across its seams
#define NB_PIECE_SITES 2048

// The openings of a piece as cutouts, in the frame of its polyhedron moved by -origin
static nbCutout* nbMakeCutouts( nbArena* arena, const nbPieceDef* piece, b3Vec3 origin )
{
	nbCutout* cutouts = nbArena_AllocArray( arena, nbCutout, piece->openingCount );
	b3Vec3 axes[3] = {
		b3RotateVector( piece->transform.q, (b3Vec3){ 1.0f, 0.0f, 0.0f } ),
		b3RotateVector( piece->transform.q, (b3Vec3){ 0.0f, 1.0f, 0.0f } ),
		b3RotateVector( piece->transform.q, (b3Vec3){ 0.0f, 0.0f, 1.0f } ),
	};

	for ( int i = 0; i < piece->openingCount; ++i )
	{
		const nbOpening* opening = piece->openings + i;
		b3Vec3 center = b3Sub( b3TransformPoint( piece->transform, opening->center ), origin );
		float halfExtents[3] = { opening->halfExtents.x, opening->halfExtents.y, opening->halfExtents.z };
		b3Vec3 extent = b3Vec3_zero;
		for ( int k = 0; k < 3; ++k )
		{
			float offset = b3Dot( axes[k], center );
			cutouts[i].planes[2 * k] = (b3Plane){ b3Neg( axes[k] ), halfExtents[k] - offset };
			cutouts[i].planes[2 * k + 1] = (b3Plane){ axes[k], halfExtents[k] + offset };
			extent = b3MulAdd( extent, halfExtents[k], b3Abs( axes[k] ) );
		}
		cutouts[i].bounds = (b3AABB){ b3Sub( center, extent ), b3Add( center, extent ) };
	}
	return cutouts;
}

static bool nbInsideCutout( const nbCutout* cutouts, int cutoutCount, b3Vec3 point )
{
	for ( int i = 0; i < cutoutCount; ++i )
	{
		bool inside = true;
		for ( int k = 0; k < 6 && inside; ++k )
		{
			inside = b3Dot( cutouts[i].planes[k].normal, point ) <= cutouts[i].planes[k].offset;
		}

		if ( inside )
		{
			return true;
		}
	}
	return false;
}

// Jittered grid sites for an even pre-fracture, none in the openings
static int nbGenerateCellSites( const nbPoly* poly, float cellSize, const nbCutout* cutouts, int cutoutCount, nbRandom* rng,
								b3Vec3* sites, int capacity )
{
	b3AABB bounds = nbPoly_ComputeBounds( poly );
	b3Vec3 extent = b3Sub( bounds.upperBound, bounds.lowerBound );
	int nx = (int)( extent.x / cellSize ) + 1;
	int ny = (int)( extent.y / cellSize ) + 1;
	int nz = (int)( extent.z / cellSize ) + 1;
	b3Vec3 step = { extent.x / (float)nx, extent.y / (float)ny, extent.z / (float)nz };

	int count = 0;
	for ( int i = 0; i < nx; ++i )
	{
		for ( int j = 0; j < ny; ++j )
		{
			for ( int k = 0; k < nz; ++k )
			{
				if ( count == capacity )
				{
					return count;
				}

				// Jitter inside the grid cell but keep away from the cell borders to avoid slivers.
				// One draw per statement: C leaves the evaluation order inside an initializer open.
				float jitterX = nbRandomRange( rng, 0.15f, 0.85f );
				float jitterY = nbRandomRange( rng, 0.15f, 0.85f );
				float jitterZ = nbRandomRange( rng, 0.15f, 0.85f );
				b3Vec3 p = {
					bounds.lowerBound.x + step.x * ( (float)i + jitterX ),
					bounds.lowerBound.y + step.y * ( (float)j + jitterY ),
					bounds.lowerBound.z + step.z * ( (float)k + jitterZ ),
				};

				if ( nbPoly_ContainsPoint( poly, p, 0.0f ) && nbInsideCutout( cutouts, cutoutCount, p ) == false )
				{
					sites[count++] = p;
				}
			}
		}
	}
	return count;
}

// Draw the pre-fracture sites of a piece into room for NB_PIECE_SITES. Returns false if the piece stays a single chunk.
static bool nbPreparePiece( nbArena* arena, const nbMaterial* material, const nbPieceDef* piece, nbPoly* poly, float cellSize,
							nbRandom* rng, b3Vec3* sites, nbFractureJob* job )
{
	int cutoutCount = piece->openings != NULL && piece->openingCount > 0 ? piece->openingCount : 0;
	if ( cellSize <= 0.0f && cutoutCount == 0 )
	{
		return false;
	}

	// Work in coordinates centered on the piece for precision
	float volume;
	b3Vec3 origin;
	nbPoly_ComputeMass( poly, &volume, &origin );
	nbPoly_Translate( poly, b3Neg( origin ) );

	const nbCutout* cutouts = cutoutCount > 0 ? nbMakeCutouts( arena, piece, origin ) : NULL;
	int siteCount = cellSize > 0.0f ? nbGenerateCellSites( poly, cellSize, cutouts, cutoutCount, rng, sites, NB_PIECE_SITES ) : 0;
	if ( siteCount < 2 )
	{
		if ( cutoutCount == 0 )
		{
			nbPoly_Translate( poly, origin );
			return false;
		}

		// A single cell, the whole piece, that the openings cut into convex parts
		sites[0] = b3Vec3_zero;
		siteCount = 1;
	}

	float radius = sqrtf( nbPoly_MaxDistanceSquared( poly, b3Vec3_zero ) );
	*job = (nbFractureJob){
		.parent = poly,
		.sites = sites,
		.siteCount = siteCount,
		.origin = origin,
		.interiorMaterial = piece->interiorMaterial,
		.tolerance = 1.0e-6f + 2.0e-6f * radius,
		.minVolume = material->minFragmentVolume,
		.buildHulls = true,
		.cutouts = cutouts,
		.cutoutCount = cutoutCount,
		.cutoutMaterial = piece->surfaceMaterial,
		.cutoutMinWidth = NB_SLIVER_WIDTH * b3MaxFloat( cellSize, 0.0f ),
	};
	return true;
}

// Glue the cells of a pre-fractured piece along their shared Voronoi faces, exactly like a single piece, given the chunk
// of every site or none. The parts of cells that openings cut, and the cells across seams, are glued by their contacts,
// see nbCommitLoad.
static void nbGlueCells( nbWorld* world, int destructibleIndex, int materialIndex, const nbFractureJob* job, const int* siteChunks )
{
	const nbMaterial* material = world->destructibles.data[destructibleIndex].materials + materialIndex;
	float fragmentSize = nbGetFragmentSize( world, material );
	float minBondArea = 0.01f * fragmentSize * fragmentSize;
	for ( int i = 0; i < job->siteCount; ++i )
	{
		if ( siteChunks[i] == NB_NULL_INDEX )
		{
			continue;
		}

		const nbCell* cell = job->cells + i;
		for ( int k = 0; k < cell->neighborCount; ++k )
		{
			const nbCellNeighbor* neighbor = cell->neighbors + k;
			int j = neighbor->site;
			if ( j <= i || siteChunks[j] == NB_NULL_INDEX || neighbor->geometry.area < minBondArea )
			{
				continue;
			}

			nbBondGeometry geometry = neighbor->geometry;
			geometry.centroid = b3Add( geometry.centroid, job->origin );
			int bondIndex = nbCreateBond( world, siteChunks[i], siteChunks[j], &geometry, material->strength * geometry.area );
			world->bonds.data[bondIndex].cohesive = true;
		}
	}
}

static bool nbMaterialEquals( const nbMaterial* a, const nbMaterial* b )
{
	return a->density == b->density && a->friction == b->friction && a->restitution == b->restitution && a->strength == b->strength &&
		   a->fragmentSize == b->fragmentSize && a->minFragmentVolume == b->minFragmentVolume && a->maxDepth == b->maxDepth &&
		   a->userMaterialId == b->userMaterialId;
}

// Index of a material of a destructible, added if it is new. When all slots are taken the piece gets the material of
// the destructible.
static int nbAddMaterial( nbMaterial* materials, int* materialCount, const nbMaterial* material )
{
	for ( int k = 0; k < *materialCount; ++k )
	{
		if ( nbMaterialEquals( materials + k, material ) )
		{
			return k;
		}
	}

	NB_ASSERT( *materialCount < NB_MAX_MATERIALS );
	if ( *materialCount == NB_MAX_MATERIALS )
	{
		return 0;
	}

	materials[*materialCount] = *material;
	return ( *materialCount )++;
}

// Floors are the flat pieces of a building, at most a quarter as high along local Y as they are wide along X and Z, and
// floors that overlap in height form one level. A storey is the room under a level, from the level below or the foot of
// the building, and remembers the volume of the chunks in it, see nbCheckStoreys. Storeys without chunks are dropped.
static void nbFindStoreys( nbWorld* world, int destructibleIndex, int actorIndex, const b3AABB* pieceBounds, const bool* valid,
						   int pieceCount )
{
	float* lows = nbArena_AllocArray( &world->arena, float, pieceCount );
	float* highs = nbArena_AllocArray( &world->arena, float, pieceCount );
	int floorCount = 0;
	float foot = FLT_MAX;
	for ( int i = 0; i < pieceCount; ++i )
	{
		if ( valid[i] == false )
		{
			continue;
		}

		b3AABB box = pieceBounds[i];
		b3Vec3 size = b3Sub( box.upperBound, box.lowerBound );
		foot = b3MinFloat( foot, box.lowerBound.y );
		if ( 4.0f * size.y > b3MinFloat( size.x, size.z ) )
		{
			continue;
		}

		// In order of height
		int k = floorCount++;
		for ( ; k > 0 && lows[k - 1] > box.lowerBound.y; --k )
		{
			lows[k] = lows[k - 1];
			highs[k] = highs[k - 1];
		}
		lows[k] = box.lowerBound.y;
		highs[k] = box.upperBound.y;
	}

	nbDestructible* destructible = world->destructibles.data + destructibleIndex;
	destructible->storeyCount = 0;
	float low = foot;
	for ( int k = 0; k < floorCount; ++k )
	{
		if ( lows[k] > low && destructible->storeyCount < NB_MAX_STOREYS )
		{
			destructible->storeys[destructible->storeyCount++] = (nbStorey){ low, lows[k], 0.0f };
		}
		low = b3MaxFloat( low, highs[k] );
	}

	const nbActor* actor = world->actors.data + actorIndex;
	for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
	{
		const nbShape* shape = world->chunks.data[c].shape;
		for ( int s = 0; s < destructible->storeyCount; ++s )
		{
			nbStorey* storey = destructible->storeys + s;
			storey->volume += shape->centroid.y >= storey->low && shape->centroid.y < storey->high ? shape->volume : 0.0f;
		}
	}

	int count = 0;
	for ( int s = 0; s < destructible->storeyCount; ++s )
	{
		if ( destructible->storeys[s].volume > 0.0f )
		{
			destructible->storeys[count++] = destructible->storeys[s];
		}
	}
	destructible->storeyCount = count;
}

// Two new chunks, by their place in the list of new chunks, first < second
typedef struct nbChunkPair
{
	int first;
	int second;
} nbChunkPair;

NB_ARRAY_DECLARE( nbChunkPair, nbChunkPairArray );

// The bounds ride along, so the sweep reads one array instead of chasing every chunk and its shape
typedef struct nbSweepEntry
{
	float lowerX;
	int place;
	b3AABB bounds;
} nbSweepEntry;

static int nbCompareSweepEntries( const void* a, const void* b )
{
	const nbSweepEntry* x = a;
	const nbSweepEntry* y = b;
	if ( x->lowerX != y->lowerX )
	{
		return x->lowerX < y->lowerX ? -1 : 1;
	}
	return ( x->place > y->place ) - ( x->place < y->place );
}

static int nbCompareChunkPairs( const void* a, const void* b )
{
	const nbChunkPair* x = a;
	const nbChunkPair* y = b;
	if ( x->first != y->first )
	{
		return x->first < y->first ? -1 : 1;
	}
	return ( x->second > y->second ) - ( x->second < y->second );
}

// One new chunk of a destructible on its way into the world, in the order the chunks are created
typedef struct nbLoadChunk
{
	nbShape* shape;
	int piece;

	// The cell of a pre-fractured piece, numbered across all pieces, or -1 for a whole piece
	int cell;

	// Chunks of one group get no bond from their contacts: the cells of a piece without openings are glued along their
	// Voronoi faces already. Every part of a piece with openings is a group of its own.
	int group;
} nbLoadChunk;

// A destructible on its way into the world. Up to the contact areas between its new chunks everything depends only on
// its definition and touches nothing of the world, so the workers can prepare the next destructible while the calling
// thread builds this one into the world, see nbCreateDestructibles. Building it in, nbCommitLoad, does everything in the
// same order as for a destructible created on its own.
typedef struct nbLoad
{
	const nbDestructibleDef* def;
	const nbPieceDef* pieces;
	int pieceCount;

	// Scratch memory of the load and of every task that computes its cells
	nbArena* arena;
	nbArena* workerArenas;

	// What the destructible gets
	nbMaterial materials[NB_MAX_MATERIALS];
	int materialCount;
	nbAnchorPlane anchors[NB_MAX_ANCHORS];
	int anchorCount;
	bool bondMoments;

	// The pieces and their pre-fracture, one job per pre-fractured piece
	int* pieceMaterials;
	nbPoly* piecePolys;
	b3AABB* pieceBounds;
	int* pieceJobs;
	float* pieceCellSizes;
	int* pieceJoints;
	nbFractureJob* jobs;
	int** jobCells;
	int jobCount;
	nbFractureRun fracture;

	// The new chunks, and for every site of a job the place of the chunk of its cell in that list, or -1
	nbLoadChunk* chunks;
	int chunkCount;
	int** jobChunks;
	int hullFallbackCount;

	// Pairs of new chunks whose bounds touch, and their contact areas, negative for two chunks of one group
	nbChunkPairArray pairs;
	float* areas;
	nbBondGeometry* geometries;
} nbLoad;

enum
{
	nb_pieceInvalid = -2,
	nb_pieceSingle = -1,
};

// Contacts closer than this are touching faces
#define NB_CONTACT_TOLERANCE 1.0e-3f

// Materials, anchors and pieces of a destructible, and the sites of its pre-fracture. Draws the sites of all pieces in
// order, so the result does not depend on the number of workers.
static void nbPrepareLoad( nbWorld* world, nbLoad* load )
{
	const nbDestructibleDef* def = load->def;
	const nbPieceDef* pieces = load->pieces;
	int pieceCount = load->pieceCount;
	nbArena* arena = load->arena;

	load->materials[0] = def->material;
	load->materialCount = 1;
	load->anchorCount = def->anchorCount < NB_MAX_ANCHORS ? def->anchorCount : NB_MAX_ANCHORS;
	for ( int i = 0; i < load->anchorCount; ++i )
	{
		load->anchors[i] = def->anchors[i];
		load->anchors[i].normal = b3Normalize( def->anchors[i].normal );
	}

	// Which destructibles have storeys is known once their chunks are, the bonds come before
	load->bondMoments = world->def.supportScale > 0.0f;

	// Default anchor: the lowest point of all pieces along local -Y
	nbPoly* poly = nbArena_AllocArray( arena, nbPoly, 1 );
	if ( def->isStatic && load->anchorCount == 0 )
	{
		float minY = FLT_MAX;
		for ( int i = 0; i < pieceCount; ++i )
		{
			const nbPieceDef* piece = pieces + i;
			if ( piece->points != NULL )
			{
				for ( int k = 0; k < piece->pointCount; ++k )
				{
					minY = b3MinFloat( minY, b3TransformPoint( piece->transform, piece->points[k] ).y );
				}
			}
			else
			{
				nbPoly_MakeBox( poly, piece->halfExtents, piece->transform, 0 );
				for ( int k = 0; k < poly->vertexCount; ++k )
				{
					minY = b3MinFloat( minY, poly->vertices[k].y );
				}
			}
		}

		load->anchors[0] = (nbAnchorPlane){ { 0.0f, 1.0f, 0.0f }, minY };
		load->anchorCount = 1;
	}

	// The material of every piece. Pieces with the same material share an entry.
	load->pieceMaterials = nbArena_AllocArray( arena, int, pieceCount );
	for ( int i = 0; i < pieceCount; ++i )
	{
		load->pieceMaterials[i] =
			pieces[i].material != NULL ? nbAddMaterial( load->materials, &load->materialCount, pieces[i].material ) : 0;
	}

	nbRandom rng = nbMakeRandom( def->seed, 0x5eed );
	load->piecePolys = nbArena_AllocArray( arena, nbPoly, pieceCount );
	load->pieceBounds = nbArena_AllocArray( arena, b3AABB, pieceCount );
	load->pieceJobs = nbArena_AllocArray( arena, int, pieceCount );
	load->pieceCellSizes = nbArena_AllocArray( arena, float, pieceCount );
	load->jobs = nbArena_AllocArray( arena, nbFractureJob, pieceCount );
	b3Vec3** jobSites = nbArena_AllocArray( arena, b3Vec3*, pieceCount );
	int* jobPieces = nbArena_AllocArray( arena, int, pieceCount );
	nbFractureJob* jobs = load->jobs;
	int jobCount = 0;
	for ( int i = 0; i < pieceCount; ++i )
	{
		const nbPieceDef* piece = pieces + i;
		nbPoly* piecePoly = load->piecePolys + i;
		bool valid = true;
		if ( piece->points != NULL )
		{
			b3HullData* hull = b3CreateHull( piece->points, piece->pointCount, B3_MAX_HULL_VERTICES );
			valid = hull != NULL && nbPoly_MakeFromHull( piecePoly, hull, piece->transform, piece->surfaceMaterial );
			if ( hull != NULL )
			{
				b3DestroyHull( hull );
			}
		}
		else
		{
			nbPoly_MakeBox( piecePoly, piece->halfExtents, piece->transform, piece->surfaceMaterial );
		}

		// The bounds in the frame of the destructible, before the fracture moves the piece to its center of mass
		load->pieceJobs[i] = valid ? nb_pieceSingle : nb_pieceInvalid;
		load->pieceBounds[i] = valid ? nbPoly_ComputeBounds( piecePoly ) : (b3AABB){ b3Vec3_zero, b3Vec3_zero };
		load->pieceCellSizes[i] = piece->cellSize != 0.0f ? piece->cellSize : def->cellSize;
		const nbMaterial* pieceMaterial = load->materials + load->pieceMaterials[i];
		b3Vec3* sites = nbArena_AllocArray( arena, b3Vec3, NB_PIECE_SITES );
		if ( valid && nbPreparePiece( arena, pieceMaterial, piece, piecePoly, load->pieceCellSizes[i], &rng, sites, jobs + jobCount ) )
		{
			jobs[jobCount].bondMoments = load->bondMoments;
			load->pieceJobs[i] = jobCount;
			jobSites[jobCount] = sites;
			jobPieces[jobCount] = i;
			jobCount += 1;
		}
	}
	load->jobCount = jobCount;

	// Pieces with cells of the same material and size crack as one mass: the first such piece, or -1
	const int* pieceJobs = load->pieceJobs;
	const float* pieceCellSizes = load->pieceCellSizes;
	const int* pieceMaterials = load->pieceMaterials;
	int* pieceJoints = nbArena_AllocArray( arena, int, pieceCount );
	for ( int i = 0; i < pieceCount; ++i )
	{
		pieceJoints[i] = -1;
		for ( int k = 0; k <= i && pieceJobs[i] >= 0 && pieceCellSizes[i] > 0.0f && pieceJoints[i] < 0; ++k )
		{
			bool same = pieceJobs[k] >= 0 && pieceCellSizes[k] == pieceCellSizes[i] && pieceMaterials[k] == pieceMaterials[i];
			pieceJoints[i] = same ? k : -1;
		}
	}
	load->pieceJoints = pieceJoints;

	// Every site is a cell, numbered across all pieces. A piece also takes the sites near it of the pieces it cracks with,
	// so the cells run on across the seam, and their parts on both sides carry the same number.
	int* ownSiteCounts = nbArena_AllocArray( arena, int, pieceCount );
	int** jobCells = nbArena_AllocArray( arena, int*, pieceCount );
	int cellCount = 0;
	for ( int j = 0; j < jobCount; ++j )
	{
		ownSiteCounts[j] = jobs[j].siteCount;
		jobCells[j] = nbArena_AllocArray( arena, int, NB_PIECE_SITES );
		for ( int s = 0; s < jobs[j].siteCount; ++s )
		{
			jobCells[j][s] = cellCount + s;
		}
		cellCount += jobs[j].siteCount;
	}
	load->jobCells = jobCells;

	for ( int j = 0; j < jobCount; ++j )
	{
		int piece = jobPieces[j];
		if ( pieceJoints[piece] < 0 )
		{
			continue;
		}

		b3AABB bounds = nbPoly_ComputeBounds( jobs[j].parent );
		float reach = NB_SEAM_REACH * pieceCellSizes[piece];
		for ( int k = 0; k < jobCount; ++k )
		{
			if ( k == j || pieceJoints[jobPieces[k]] != pieceJoints[piece] )
			{
				continue;
			}

			b3Vec3 offset = b3Sub( jobs[k].origin, jobs[j].origin );
			for ( int s = 0; s < ownSiteCounts[k] && jobs[j].siteCount < NB_PIECE_SITES; ++s )
			{
				b3Vec3 site = b3Add( jobSites[k][s], offset );
				b3Vec3 below = b3Sub( bounds.lowerBound, site );
				b3Vec3 above = b3Sub( site, bounds.upperBound );
				if ( b3MaxFloat( b3MaxFloat( below.x, above.x ),
								 b3MaxFloat( b3MaxFloat( below.y, above.y ), b3MaxFloat( below.z, above.z ) ) ) <= reach )
				{
					jobCells[j][jobs[j].siteCount] = jobCells[k][s];
					jobSites[j][jobs[j].siteCount] = site;
					jobs[j].siteCount += 1;
				}
			}
		}
	}
}

static void nbAddLoadChunk( nbLoad* load, nbShape* shape, int piece, int cell, bool cut )
{
	int place = load->chunkCount++;
	load->chunks[place] = (nbLoadChunk){ shape, piece, cell, cut ? load->pieceCount + place : piece };
}

// The new chunks in the order they are created: the pieces in order, a whole piece as one chunk, a pre-fractured piece
// as the parts and then the cell of every site. Shapes without a hull, slivers that have no valid one, are dropped.
static void nbCollectLoadChunks( nbLoad* load )
{
	int capacity = 0;
	for ( int i = 0; i < load->pieceCount; ++i )
	{
		int jobIndex = load->pieceJobs[i];
		if ( jobIndex == nb_pieceSingle )
		{
			capacity += 1;
		}
		else if ( jobIndex >= 0 )
		{
			const nbFractureJob* job = load->jobs + jobIndex;
			for ( int s = 0; s < job->siteCount; ++s )
			{
				capacity += job->cells[s].partCount + 1;
			}
		}
	}

	load->chunks = nbArena_AllocArray( load->arena, nbLoadChunk, capacity + 1 );
	load->chunkCount = 0;
	load->jobChunks = nbArena_AllocArray( load->arena, int*, load->jobCount + 1 );
	for ( int i = 0; i < load->pieceCount; ++i )
	{
		int jobIndex = load->pieceJobs[i];
		if ( jobIndex == nb_pieceInvalid )
		{
			continue;
		}

		if ( jobIndex == nb_pieceSingle )
		{
			nbShape* shape = nbShape_Create( load->piecePolys + i );
			if ( shape == NULL )
			{
				continue;
			}

			nbShape_BuildHull( shape, &load->hullFallbackCount );
			if ( shape->hull == NULL )
			{
				nbShape_Destroy( shape );
				continue;
			}
			nbAddLoadChunk( load, shape, i, -1, false );
			continue;
		}

		// The chunks of a piece with openings count as pieces of their own, the cells do not know how their parts touch
		const nbFractureJob* job = load->jobs + jobIndex;
		const int* cells = load->jobCells[jobIndex];
		bool cut = job->cutoutCount > 0;
		int* siteChunks = nbArena_AllocArray( load->arena, int, job->siteCount );
		load->jobChunks[jobIndex] = siteChunks;
		for ( int s = 0; s < job->siteCount; ++s )
		{
			siteChunks[s] = -1;
			const nbCell* cell = job->cells + s;
			for ( int k = 0; k < cell->partCount; ++k )
			{
				nbShape* part = cell->parts[k].shape;
				if ( part->hull == NULL )
				{
					nbShape_Destroy( part );
					continue;
				}
				nbAddLoadChunk( load, part, i, cells[s], cut );
			}

			if ( cell->shape == NULL )
			{
				continue;
			}

			if ( cell->shape->hull == NULL )
			{
				nbShape_Destroy( cell->shape );
				continue;
			}
			siteChunks[s] = load->chunkCount;
			nbAddLoadChunk( load, cell->shape, i, cells[s], cut );
		}
	}
}

// The pairs of new chunks whose bounds pass the test nbShape_ContactArea starts with, in the order of a loop over all
// pairs. A sweep along x finds them without testing every pair, a house has hundreds of chunks.
static void nbFindTouchingPairs( nbLoad* load, float tolerance )
{
	int count = load->chunkCount;
	nbChunkPairArray* pairs = &load->pairs;
	if ( count < 2 )
	{
		return;
	}

	nbSweepEntry* entries = nbArena_AllocArray( load->arena, nbSweepEntry, count );
	for ( int i = 0; i < count; ++i )
	{
		b3AABB bounds = load->chunks[i].shape->bounds;
		entries[i] = (nbSweepEntry){ bounds.lowerBound.x, i, bounds };
	}
	qsort( entries, (size_t)count, sizeof( nbSweepEntry ), nbCompareSweepEntries );

	for ( int i = 0; i < count; ++i )
	{
		int place = entries[i].place;
		b3AABB bounds = entries[i].bounds;

		// Twice the tolerance keeps every pair the exact test accepts, whatever the rounding
		float reach = bounds.upperBound.x + 2.0f * tolerance;
		for ( int j = i + 1; j < count && entries[j].lowerX <= reach; ++j )
		{
			int other = entries[j].place;
			bool before = place < other;
			const b3AABB* first = before ? &bounds : &entries[j].bounds;
			const b3AABB* second = before ? &entries[j].bounds : &bounds;
			if ( b3AABB_Overlaps( b3AABB_Inflate( *first, tolerance ), *second ) )
			{
				nbChunkPair pair = { before ? place : other, before ? other : place };
				nbArray_Push( *pairs, pair );
			}
		}
	}

	if ( pairs->count > 1 )
	{
		qsort( pairs->data, (size_t)pairs->count, sizeof( nbChunkPair ), nbCompareChunkPairs );
	}
}

// Pairs per work item. A contact area costs a microsecond or two, a single pair is not worth an atomic.
#define NB_CONTACT_BLOCK 16

// Blocks from which the workers help. Waking them costs more than a few blocks.
#define NB_PARALLEL_CONTACT_BLOCKS 8

// The contact areas of a block of touching pairs, measured on the workers. Glueing the pairs stays on the calling thread,
// in pair order, so the bonds come out the same with any number of threads.
static void nbMeasureContacts( void* context, int item )
{
	nbLoad* load = context;
	int begin = item * NB_CONTACT_BLOCK;
	int end = begin + NB_CONTACT_BLOCK < load->pairs.count ? begin + NB_CONTACT_BLOCK : load->pairs.count;
	for ( int i = begin; i < end; ++i )
	{
		const nbLoadChunk* chunkA = load->chunks + load->pairs.data[i].first;
		const nbLoadChunk* chunkB = load->chunks + load->pairs.data[i].second;
		if ( chunkA->group == chunkB->group )
		{
			load->areas[i] = -1.0f;
			continue;
		}

		load->areas[i] =
			nbShape_ContactArea( chunkA->shape, chunkB->shape, NB_CONTACT_TOLERANCE, load->bondMoments, load->geometries + i );
	}
}

// Find the pairs of new chunks that touch and measure their contact areas on the workers
static void nbMeasureLoadContacts( nbWorld* world, nbLoad* load )
{
	nbFindTouchingPairs( load, NB_CONTACT_TOLERANCE );
	load->areas = nbArena_AllocArray( load->arena, float, load->pairs.count + 1 );
	load->geometries = nbArena_AllocArray( load->arena, nbBondGeometry, load->pairs.count + 1 );
	int blockCount = ( load->pairs.count + NB_CONTACT_BLOCK - 1 ) / NB_CONTACT_BLOCK;
	nbParallelFor( world, blockCount, NB_PARALLEL_CONTACT_BLOCKS, nbMeasureContacts, load );
}

// Build a prepared destructible into the world: its slot and actor, the chunks in order, the bonds of the cells of every
// piece right after its chunks, then the bonds of the touching pairs in pair order, the storeys and the Box3D shapes.
// A bond between two materials is as strong as the weaker one. Cells of one piece, or of pieces that crack together,
// hold as one block, and the parts of one cell hold as siblings.
static nbDestructibleId nbCommitLoad( nbWorld* world, nbLoad* load )
{
	const nbDestructibleDef* def = load->def;
	int pieceCount = load->pieceCount;
	world->touchedChunks.count = 0;
	world->touchedActors.count = 0;
	world->splitSeeds.count = 0;

	int index;
	if ( world->freeDestructibles.count > 0 )
	{
		index = world->freeDestructibles.data[--world->freeDestructibles.count];
	}
	else
	{
		nbDestructible empty = { 0 };
		nbArray_Push( world->destructibles, empty );
		index = world->destructibles.count - 1;
	}

	nbDestructible* destructible = world->destructibles.data + index;
	uint16_t generation = destructible->generation;
	*destructible = (nbDestructible){ 0 };
	destructible->generation = generation;
	memcpy( destructible->materials, load->materials, sizeof( nbMaterial ) * (size_t)load->materialCount );
	destructible->materialCount = load->materialCount;
	destructible->filter = def->filter;
	memcpy( destructible->anchors, load->anchors, sizeof( nbAnchorPlane ) * (size_t)load->anchorCount );
	destructible->anchorCount = load->anchorCount;
	destructible->headActor = NB_NULL_INDEX;
	destructible->transform = (b3WorldTransform){ def->position, def->rotation };
	destructible->staticBody = b3_nullBodyId;
	destructible->seed = def->seed;
	destructible->isStatic = def->isStatic;
	destructible->enableCollisionDamage = def->enableCollisionDamage;
	destructible->userData = def->userData;
	destructible->bondMoments = load->bondMoments;
	world->destructibleCount += 1;
	world->stats.hullFallbackCount += load->hullFallbackCount;

	// Static chunks get a body each when they are committed. A dynamic destructible is one body.
	int actorIndex = nbAllocActor( world, index, def->isStatic );
	if ( def->isStatic == false )
	{
		nbCreateActorBody( world, actorIndex, destructible->transform );
	}
	world->actors.data[actorIndex].isNew = false;

	int* chunkIndices = nbArena_AllocArray( load->arena, int, load->chunkCount + 1 );
	int place = 0;
	for ( int i = 0; i < pieceCount; ++i )
	{
		int jobIndex = load->pieceJobs[i];
		const nbFractureJob* job = jobIndex >= 0 ? load->jobs + jobIndex : NULL;
		uint8_t interiorMaterial = job != NULL ? job->interiorMaterial : load->pieces[i].interiorMaterial;
		int first = place;
		for ( ; place < load->chunkCount && load->chunks[place].piece == i; ++place )
		{
			chunkIndices[place] = nbCreateChunkWithHull( world, index, actorIndex, load->chunks[place].shape, 0, interiorMaterial,
														 load->pieceMaterials[i] );
			NB_ASSERT( chunkIndices[place] != NB_NULL_INDEX );
		}

		if ( job == NULL || job->cutoutCount > 0 )
		{
			continue;
		}

		const int* siteChunks = load->jobChunks[jobIndex];
		int* siteIndices = nbArena_AllocArray( load->arena, int, job->siteCount );
		for ( int s = 0; s < job->siteCount; ++s )
		{
			NB_ASSERT( siteChunks[s] < 0 || ( first <= siteChunks[s] && siteChunks[s] < place ) );
			siteIndices[s] = siteChunks[s] >= 0 ? chunkIndices[siteChunks[s]] : NB_NULL_INDEX;
		}
		NB_UNUSED( first );
		nbGlueCells( world, index, load->pieceMaterials[i], job, siteIndices );
	}

	const int* pieceJoints = load->pieceJoints;
	for ( int i = 0; i < load->pairs.count; ++i )
	{
		float area = load->areas[i];
		if ( area < 0.0f )
		{
			continue;
		}

		const nbLoadChunk* loadA = load->chunks + load->pairs.data[i].first;
		const nbLoadChunk* loadB = load->chunks + load->pairs.data[i].second;
		int chunkA = chunkIndices[load->pairs.data[i].first];
		int chunkB = chunkIndices[load->pairs.data[i].second];
		const nbMaterial* materialA = nbGetChunkMaterial( world, world->chunks.data + chunkA );
		const nbMaterial* materialB = nbGetChunkMaterial( world, world->chunks.data + chunkB );
		float fragmentSize = b3MinFloat( nbGetFragmentSize( world, materialA ), nbGetFragmentSize( world, materialB ) );
		float minBondArea = 0.01f * fragmentSize * fragmentSize;
		if ( area > minBondArea )
		{
			int bondIndex = nbCreateBond( world, chunkA, chunkB, load->geometries + i,
										  b3MinFloat( materialA->strength, materialB->strength ) * area );

			int pieceA = loadA->piece, pieceB = loadB->piece;
			int cellA = loadA->cell, cellB = loadB->cell;
			bool joint = pieceA == pieceB || ( pieceJoints[pieceA] >= 0 && pieceJoints[pieceA] == pieceJoints[pieceB] );
			world->bonds.data[bondIndex].cohesive = joint && cellA >= 0 && cellB >= 0;
			world->bonds.data[bondIndex].sibling = joint && cellA >= 0 && cellA == cellB;
		}
	}
	nbArray_Free( load->pairs );

	if ( def->isStatic )
	{
		bool* valid = nbArena_AllocArray( load->arena, bool, pieceCount );
		for ( int i = 0; i < pieceCount; ++i )
		{
			valid[i] = load->pieceJobs[i] != nb_pieceInvalid;
		}
		nbFindStoreys( world, index, actorIndex, load->pieceBounds, valid, pieceCount );
	}

	// The load check never takes a building with storeys, its bonds need no moments from now on
	nbDestructible* built = world->destructibles.data + index;
	built->bondMoments = built->bondMoments && built->storeyCount == 0;

	nbCommitPhysics( world );
	world->touchedActors.count = 0;
	nbCoverIndices( world );

	return (nbDestructibleId){ index + 1, world->worldIndex, world->destructibles.data[index].generation };
}

nbDestructibleId nbCreateDestructible( nbWorldId worldId, const nbDestructibleDef* def, const nbPieceDef* pieces, int pieceCount )
{
	NB_ASSERT( def->internalValue == NB_SECRET_COOKIE );
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL || pieces == NULL || pieceCount <= 0 )
	{
		return nb_nullDestructibleId;
	}

	nbBeginOperation( world );
	nbLoad load = {
		.def = def,
		.pieces = pieces,
		.pieceCount = pieceCount,
		.arena = &world->arena,
		.workerArenas = world->workerArenas,
	};
	nbPrepareLoad( world, &load );
	nbStartFractureJobs( world, &load.fracture, load.jobs, load.jobCount, NULL, NULL, load.arena, load.workerArenas );
	nbFinishFractureJobs( world, &load.fracture );
	nbCollectLoadChunks( &load );
	nbMeasureLoadContacts( world, &load );
	return nbCommitLoad( world, &load );
}

void nbCreateDestructibles( nbWorldId worldId, const nbDestructibleDef* defs, const nbPieceDef* const* pieceLists,
							const int* pieceCounts, int count, nbDestructibleId* ids )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	for ( int i = 0; i < count; ++i )
	{
		ids[i] = nb_nullDestructibleId;
	}

	if ( world == NULL || count <= 0 )
	{
		return;
	}

	// The second load works in scratch memory of its own, the first in that of the world
	nbArena arena;
	nbArena workerArenas[NB_MAX_WORKERS];
	nbArena_Create( &arena, 256 * 1024 );
	for ( int i = 0; i < world->workerCount; ++i )
	{
		nbArena_Create( workerArenas + i, 64 * 1024 );
	}

	nbLoad loads[2];
	nbBeginOperation( world );
	int current = 0;
	while ( current < count && ( pieceLists[current] == NULL || pieceCounts[current] <= 0 ) )
	{
		current += 1;
	}

	int side = 0;
	if ( current < count )
	{
		NB_ASSERT( defs[current].internalValue == NB_SECRET_COOKIE );
		loads[0] = (nbLoad){ .def = defs + current, .pieces = pieceLists[current], .pieceCount = pieceCounts[current],
							 .arena = &world->arena, .workerArenas = world->workerArenas };
		nbPrepareLoad( world, loads + 0 );
		nbStartFractureJobs( world, &loads[0].fracture, loads[0].jobs, loads[0].jobCount, NULL, NULL, loads[0].arena,
							 loads[0].workerArenas );
	}

	// While the calling thread builds one destructible into the world, the workers compute the cells of the next
	while ( current < count )
	{
		nbLoad* load = loads + side;
		nbFinishFractureJobs( world, &load->fracture );

		int next = current + 1;
		while ( next < count && ( pieceLists[next] == NULL || pieceCounts[next] <= 0 ) )
		{
			next += 1;
		}

		if ( next < count )
		{
			NB_ASSERT( defs[next].internalValue == NB_SECRET_COOKIE );
			nbLoad* other = loads + ( 1 - side );
			nbArena* otherArena = side == 0 ? &arena : &world->arena;
			nbArena* otherWorkerArenas = side == 0 ? workerArenas : world->workerArenas;
			nbArena_Reset( otherArena );
			for ( int i = 0; i < world->workerCount; ++i )
			{
				nbArena_Reset( otherWorkerArenas + i );
			}

			*other = (nbLoad){ .def = defs + next, .pieces = pieceLists[next], .pieceCount = pieceCounts[next],
							   .arena = otherArena, .workerArenas = otherWorkerArenas };
			nbPrepareLoad( world, other );
			nbStartFractureJobs( world, &other->fracture, other->jobs, other->jobCount, NULL, NULL, other->arena,
								 other->workerArenas );
		}

		// The workers are busy with the cells of the next destructible, the contacts of this one come mostly from the
		// calling thread
		nbCollectLoadChunks( load );
		nbMeasureLoadContacts( world, load );
		ids[current] = nbCommitLoad( world, load );
		current = next;
		side = 1 - side;
	}

	nbArena_Destroy( &arena );
	for ( int i = 0; i < world->workerCount; ++i )
	{
		nbArena_Destroy( workerArenas + i );
	}
}

nbDestructibleId nbCreateBox( nbWorldId worldId, const nbDestructibleDef* def, b3Vec3 halfExtents )
{
	nbPieceDef piece = nbDefaultPieceDef();
	piece.halfExtents = halfExtents;
	return nbCreateDestructible( worldId, def, &piece, 1 );
}

void nbDestroyDestructible( nbDestructibleId destructibleId )
{
	nbWorld* world;
	nbDestructible* destructible = nbGetDestructibleFromId( destructibleId, &world );
	if ( destructible == NULL )
	{
		return;
	}

	int index = destructibleId.index1 - 1;
	nbDestroyDestructibleParts( world, index );

	// The shared static body, without shapes by now
	destructible = world->destructibles.data + index;
	if ( B3_IS_NON_NULL( destructible->staticBody ) && b3Body_IsValid( destructible->staticBody ) )
	{
		b3DestroyBody( destructible->staticBody );
	}
	destructible->staticBody = b3_nullBodyId;

	destructible->isFree = true;
	destructible->generation += 1;
	nbArray_Push( world->freeDestructibles, index );
	world->destructibleCount -= 1;
}

bool nbDestructible_IsValid( nbDestructibleId destructibleId )
{
	return nbGetDestructibleFromId( destructibleId, NULL ) != NULL;
}

int nbDestructible_GetChunkCount( nbDestructibleId destructibleId )
{
	nbDestructible* destructible = nbGetDestructibleFromId( destructibleId, NULL );
	return destructible != NULL ? destructible->chunkCount : 0;
}

int nbDestructible_GetChunks( nbDestructibleId destructibleId, nbChunkId* chunks, int capacity )
{
	nbWorld* world;
	nbDestructible* destructible = nbGetDestructibleFromId( destructibleId, &world );
	if ( destructible == NULL )
	{
		return 0;
	}

	int count = 0;
	for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
	{
		for ( int c = world->actors.data[a].headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
		{
			if ( count == capacity )
			{
				return count;
			}
			chunks[count++] = nbMakeChunkId( world, c );
		}
	}
	return count;
}

void* nbDestructible_GetUserData( nbDestructibleId destructibleId )
{
	nbDestructible* destructible = nbGetDestructibleFromId( destructibleId, NULL );
	return destructible != NULL ? destructible->userData : NULL;
}

bool nbDestructible_IsIntact( nbDestructibleId destructibleId )
{
	nbDestructible* destructible = nbGetDestructibleFromId( destructibleId, NULL );
	return destructible != NULL && destructible->damaged == false;
}

typedef struct nbUnloadQuery
{
	nbWorld* world;
	int destructibleIndex;
	bool blocked;
} nbUnloadQuery;

// Whatever lies near a destructible keeps it from unloading, unless it stands for itself: its own chunks, the standing
// parts of other destructibles and the static bodies of the application. Debris, rubble and anything that moves or can be
// moved could lie on it or hit it.
static bool nbUnloadQueryCallback( b3ShapeId shapeId, void* context )
{
	nbUnloadQuery* query = context;
	if ( b3Shape_IsSensor( shapeId ) )
	{
		return true;
	}

	nbWorld* world = query->world;
	int chunkIndex = nbFindChunkFromShape( world, shapeId );
	if ( chunkIndex != NB_NULL_INDEX )
	{
		const nbChunk* chunk = world->chunks.data + chunkIndex;
		if ( chunk->destructibleIndex == query->destructibleIndex || world->actors.data[chunk->actorIndex].isStatic )
		{
			return true;
		}
	}
	else if ( b3Body_GetType( b3Shape_GetBody( shapeId ) ) == b3_staticBody )
	{
		return true;
	}

	query->blocked = true;
	return false;
}

bool nbDestructible_CanUnload( nbDestructibleId destructibleId, float margin )
{
	nbWorld* world;
	nbDestructible* destructible = nbGetDestructibleFromId( destructibleId, &world );
	if ( destructible == NULL || destructible->isStatic == false || destructible->damaged || destructible->actorCount != 1 ||
		 destructible->chunkCount == 0 )
	{
		return false;
	}

	const nbActor* actor = world->actors.data + destructible->headActor;
	if ( actor->isStatic == false )
	{
		return false;
	}

	b3AABB bounds = { { FLT_MAX, FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX, -FLT_MAX } };
#if defined( NB_SHARED_STATIC_BODY )
	bounds = b3Body_ComputeAABB( destructible->staticBody );
#else
	for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
	{
		bounds = b3AABB_Union( bounds, b3Shape_GetAABB( world->chunks.data[c].shapeId ) );
	}
#endif

	margin = b3MaxFloat( margin, 0.0f );
	b3Vec3 extent = { margin, margin, margin };
	bounds.lowerBound = b3Sub( bounds.lowerBound, extent );
	bounds.upperBound = b3Add( bounds.upperBound, extent );

	// Only what collides with the chunks counts
	b3QueryFilter filter = b3DefaultQueryFilter();
	filter.categoryBits = destructible->filter.categoryBits;
	filter.maskBits = destructible->filter.maskBits;
	nbUnloadQuery query = { world, destructibleId.index1 - 1, false };
	b3World_OverlapAABB( world->physicsWorld, bounds, filter, nbUnloadQueryCallback, &query );
	return query.blocked == false;
}
