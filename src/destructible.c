// SPDX-License-Identifier: MIT

#include "fracture.h"
#include "world.h"

#include <float.h>

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
static nbCutout* nbMakeCutouts( nbWorld* world, const nbPieceDef* piece, b3Vec3 origin )
{
	nbCutout* cutouts = nbArena_AllocArray( &world->arena, nbCutout, piece->openingCount );
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
static bool nbPreparePiece( nbWorld* world, const nbMaterial* material, const nbPieceDef* piece, nbPoly* poly, float cellSize,
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

	const nbCutout* cutouts = cutoutCount > 0 ? nbMakeCutouts( world, piece, origin ) : NULL;
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

static void nbCreateSingleChunk( nbWorld* world, int destructibleIndex, int actorIndex, const nbPoly* poly, uint8_t interiorMaterial,
								 int materialIndex )
{
	nbShape* shape = nbShape_Create( poly );
	if ( shape != NULL )
	{
		nbCreateChunk( world, destructibleIndex, actorIndex, shape, 0, interiorMaterial, materialIndex );
	}
}

// Create the chunks of a pre-fractured piece and glue them exactly like a single piece, with the cell of every new chunk in
// the order they are created. The parts of cells that openings cut, and the cells across seams, are glued by
// nbCreateDestructible.
static void nbFinishPiece( nbWorld* world, int destructibleIndex, int actorIndex, const nbFractureJob* job, const int* cells,
						   int materialIndex, nbIntArray* chunkCells )
{
	const nbMaterial* material = world->destructibles.data[destructibleIndex].materials + materialIndex;

	int* chunkIndices = nbArena_AllocArray( &world->arena, int, job->siteCount );
	for ( int i = 0; i < job->siteCount; ++i )
	{
		chunkIndices[i] = NB_NULL_INDEX;
		const nbCell* cell = job->cells + i;
		for ( int k = 0; k < cell->partCount; ++k )
		{
			int chunkIndex = nbCreateChunkWithHull( world, destructibleIndex, actorIndex, cell->parts[k].shape,
													cell->parts[k].hull, 0, job->interiorMaterial, materialIndex );
			if ( chunkIndex != NB_NULL_INDEX )
			{
				nbArray_Push( *chunkCells, cells[i] );
			}
		}

		if ( cell->shape == NULL )
		{
			continue;
		}

		chunkIndices[i] =
			nbCreateChunkWithHull( world, destructibleIndex, actorIndex, cell->shape, cell->hull, 0, job->interiorMaterial, materialIndex );
		if ( chunkIndices[i] != NB_NULL_INDEX )
		{
			nbArray_Push( *chunkCells, cells[i] );
		}
	}

	if ( job->cutoutCount > 0 )
	{
		return;
	}

	float fragmentSize = nbGetFragmentSize( world, material );
	float minBondArea = 0.01f * fragmentSize * fragmentSize;
	for ( int i = 0; i < job->siteCount; ++i )
	{
		if ( chunkIndices[i] == NB_NULL_INDEX )
		{
			continue;
		}

		const nbCell* cell = job->cells + i;
		for ( int k = 0; k < cell->neighborCount; ++k )
		{
			const nbCellNeighbor* neighbor = cell->neighbors + k;
			int j = neighbor->site;
			if ( j <= i || chunkIndices[j] == NB_NULL_INDEX || neighbor->geometry.area < minBondArea )
			{
				continue;
			}

			nbBondGeometry geometry = neighbor->geometry;
			geometry.centroid = b3Add( geometry.centroid, job->origin );
			int bondIndex =
				nbCreateBond( world, chunkIndices[i], chunkIndices[j], &geometry, material->strength * geometry.area );
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

// Index of a material in the destructible, added if it is new. When all slots are taken the piece gets the
// material of the destructible.
static int nbAddMaterial( nbDestructible* destructible, const nbMaterial* material )
{
	for ( int k = 0; k < destructible->materialCount; ++k )
	{
		if ( nbMaterialEquals( destructible->materials + k, material ) )
		{
			return k;
		}
	}

	NB_ASSERT( destructible->materialCount < NB_MAX_MATERIALS );
	if ( destructible->materialCount == NB_MAX_MATERIALS )
	{
		return 0;
	}

	destructible->materials[destructible->materialCount] = *material;
	return destructible->materialCount++;
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

nbDestructibleId nbCreateDestructible( nbWorldId worldId, const nbDestructibleDef* def, const nbPieceDef* pieces, int pieceCount )
{
	NB_ASSERT( def->internalValue == NB_SECRET_COOKIE );
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL || pieces == NULL || pieceCount <= 0 )
	{
		return nb_nullDestructibleId;
	}

	nbBeginOperation( world );

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
	destructible->materials[0] = def->material;
	destructible->materialCount = 1;
	destructible->filter = def->filter;
	destructible->anchorCount = def->anchorCount < NB_MAX_ANCHORS ? def->anchorCount : NB_MAX_ANCHORS;
	for ( int i = 0; i < destructible->anchorCount; ++i )
	{
		destructible->anchors[i] = def->anchors[i];
		destructible->anchors[i].normal = b3Normalize( def->anchors[i].normal );
	}
	destructible->headActor = NB_NULL_INDEX;
	destructible->transform = (b3WorldTransform){ def->position, def->rotation };
	destructible->seed = def->seed;
	destructible->isStatic = def->isStatic;
	destructible->enableCollisionDamage = def->enableCollisionDamage;
	destructible->userData = def->userData;
	world->destructibleCount += 1;

	// Static chunks get a body each when they are committed. A dynamic destructible is one body.
	int actorIndex = nbAllocActor( world, index, def->isStatic );
	if ( def->isStatic == false )
	{
		nbCreateActorBody( world, actorIndex, destructible->transform );
	}
	world->actors.data[actorIndex].isNew = false;

	// Default anchor: the lowest point of all pieces along local -Y
	nbPoly* poly = nbArena_AllocArray( &world->arena, nbPoly, 1 );
	if ( def->isStatic && destructible->anchorCount == 0 )
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

		destructible->anchors[0] = (nbAnchorPlane){ { 0.0f, 1.0f, 0.0f }, minY };
		destructible->anchorCount = 1;
	}

	// The material of every piece. Pieces with the same material share an entry.
	int* pieceMaterials = nbArena_AllocArray( &world->arena, int, pieceCount );
	for ( int i = 0; i < pieceCount; ++i )
	{
		pieceMaterials[i] = pieces[i].material != NULL ? nbAddMaterial( world->destructibles.data + index, pieces[i].material ) : 0;
	}

	// Draw the sites of all pieces in order, compute the cells on the workers, then create the chunks in
	// order. The result does not depend on the number of workers.
	nbRandom rng = nbMakeRandom( def->seed, 0x5eed );
	int firstNewChunk = world->touchedChunks.count;

	enum
	{
		nb_pieceInvalid = -2,
		nb_pieceSingle = -1,
	};

	nbPoly* piecePolys = nbArena_AllocArray( &world->arena, nbPoly, pieceCount );
	b3AABB* pieceBounds = nbArena_AllocArray( &world->arena, b3AABB, pieceCount );
	int* pieceJobs = nbArena_AllocArray( &world->arena, int, pieceCount );
	float* pieceCellSizes = nbArena_AllocArray( &world->arena, float, pieceCount );
	nbFractureJob* jobs = nbArena_AllocArray( &world->arena, nbFractureJob, pieceCount );
	b3Vec3** jobSites = nbArena_AllocArray( &world->arena, b3Vec3*, pieceCount );
	int* jobPieces = nbArena_AllocArray( &world->arena, int, pieceCount );
	int jobCount = 0;

	for ( int i = 0; i < pieceCount; ++i )
	{
		const nbPieceDef* piece = pieces + i;
		nbPoly* piecePoly = piecePolys + i;
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
		pieceJobs[i] = valid ? nb_pieceSingle : nb_pieceInvalid;
		pieceBounds[i] = valid ? nbPoly_ComputeBounds( piecePoly ) : (b3AABB){ b3Vec3_zero, b3Vec3_zero };
		pieceCellSizes[i] = piece->cellSize != 0.0f ? piece->cellSize : def->cellSize;
		const nbMaterial* pieceMaterial = world->destructibles.data[index].materials + pieceMaterials[i];
		b3Vec3* sites = nbArena_AllocArray( &world->arena, b3Vec3, NB_PIECE_SITES );
		if ( valid && nbPreparePiece( world, pieceMaterial, piece, piecePoly, pieceCellSizes[i], &rng, sites, jobs + jobCount ) )
		{
			pieceJobs[i] = jobCount;
			jobSites[jobCount] = sites;
			jobPieces[jobCount] = i;
			jobCount += 1;
		}
	}

	// Pieces with cells of the same material and size crack as one mass: the first such piece, or -1
	int* pieceJoints = nbArena_AllocArray( &world->arena, int, pieceCount );
	for ( int i = 0; i < pieceCount; ++i )
	{
		pieceJoints[i] = -1;
		for ( int k = 0; k <= i && pieceJobs[i] >= 0 && pieceCellSizes[i] > 0.0f && pieceJoints[i] < 0; ++k )
		{
			bool same = pieceJobs[k] >= 0 && pieceCellSizes[k] == pieceCellSizes[i] && pieceMaterials[k] == pieceMaterials[i];
			pieceJoints[i] = same ? k : -1;
		}
	}

	// Every site is a cell, numbered across all pieces. A piece also takes the sites near it of the pieces it cracks with,
	// so the cells run on across the seam, and their parts on both sides carry the same number.
	int* ownSiteCounts = nbArena_AllocArray( &world->arena, int, pieceCount );
	int** jobCells = nbArena_AllocArray( &world->arena, int*, pieceCount );
	int cellCount = 0;
	for ( int j = 0; j < jobCount; ++j )
	{
		ownSiteCounts[j] = jobs[j].siteCount;
		jobCells[j] = nbArena_AllocArray( &world->arena, int, NB_PIECE_SITES );
		for ( int s = 0; s < jobs[j].siteCount; ++s )
		{
			jobCells[j][s] = cellCount + s;
		}
		cellCount += jobs[j].siteCount;
	}

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

	nbRunFractureJobs( world, jobs, jobCount, NULL, NULL, NULL, NULL );

	// The piece and cell of every new chunk, in the order of the touched chunks. A chunk that is a whole piece has no cell.
	nbIntArray chunkPieces = { 0 };
	nbIntArray chunkCells = { 0 };
	for ( int i = 0; i < pieceCount; ++i )
	{
		if ( pieceJobs[i] == nb_pieceInvalid )
		{
			continue;
		}

		int start = world->touchedChunks.count;
		if ( pieceJobs[i] == nb_pieceSingle )
		{
			nbCreateSingleChunk( world, index, actorIndex, piecePolys + i, pieces[i].interiorMaterial, pieceMaterials[i] );
			for ( int k = start; k < world->touchedChunks.count; ++k )
			{
				nbArray_Push( chunkCells, -1 );
			}
		}
		else
		{
			nbFinishPiece( world, index, actorIndex, jobs + pieceJobs[i], jobCells[pieceJobs[i]], pieceMaterials[i],
						   &chunkCells );
		}

		// Remember which piece each chunk came from, so bonds between pieces can be found. The chunks of a piece with
		// openings count as pieces of their own, the cells do not know how their parts touch.
		bool cut = pieceJobs[i] >= 0 && jobs[pieceJobs[i]].cutoutCount > 0;
		for ( int k = start; k < world->touchedChunks.count; ++k )
		{
			world->chunks.data[world->touchedChunks.data[k]].scratch = cut ? pieceCount + k : i;
			nbArray_Push( chunkPieces, i );
		}
	}

	// Glue touching faces of different pieces. A bond between two materials is as strong as the weaker one. Cells of one
	// piece, or of pieces that crack together, hold as one block, and the parts of one cell hold as siblings.
	int chunkEnd = world->touchedChunks.count;
	for ( int a = firstNewChunk; a < chunkEnd; ++a )
	{
		int chunkA = world->touchedChunks.data[a];
		int groupA = world->chunks.data[chunkA].scratch;
		for ( int b = a + 1; b < chunkEnd; ++b )
		{
			int chunkB = world->touchedChunks.data[b];
			int groupB = world->chunks.data[chunkB].scratch;
			if ( groupA == groupB )
			{
				continue;
			}

			const nbMaterial* materialA = nbGetChunkMaterial( world, world->chunks.data + chunkA );
			const nbMaterial* materialB = nbGetChunkMaterial( world, world->chunks.data + chunkB );
			float fragmentSize = b3MinFloat( nbGetFragmentSize( world, materialA ), nbGetFragmentSize( world, materialB ) );
			float minBondArea = 0.01f * fragmentSize * fragmentSize;

			nbBondGeometry geometry;
			float area = nbShape_ContactArea( world->chunks.data[chunkA].shape, world->chunks.data[chunkB].shape, 1.0e-3f, &geometry );
			if ( area > minBondArea )
			{
				int bondIndex = nbCreateBond( world, chunkA, chunkB, &geometry,
											  b3MinFloat( materialA->strength, materialB->strength ) * area );

				int pieceA = chunkPieces.data[a - firstNewChunk], pieceB = chunkPieces.data[b - firstNewChunk];
				int cellA = chunkCells.data[a - firstNewChunk], cellB = chunkCells.data[b - firstNewChunk];
				bool joint = pieceA == pieceB || ( pieceJoints[pieceA] >= 0 && pieceJoints[pieceA] == pieceJoints[pieceB] );
				world->bonds.data[bondIndex].cohesive = joint && cellA >= 0 && cellB >= 0;
				world->bonds.data[bondIndex].sibling = joint && cellA >= 0 && cellA == cellB;
			}
		}
	}
	nbArray_Free( chunkPieces );
	nbArray_Free( chunkCells );

	for ( int k = firstNewChunk; k < chunkEnd; ++k )
	{
		world->chunks.data[world->touchedChunks.data[k]].scratch = NB_NULL_INDEX;
	}

	if ( def->isStatic )
	{
		bool* valid = nbArena_AllocArray( &world->arena, bool, pieceCount );
		for ( int i = 0; i < pieceCount; ++i )
		{
			valid[i] = pieceJobs[i] != nb_pieceInvalid;
		}
		nbFindStoreys( world, index, actorIndex, pieceBounds, valid, pieceCount );
	}

	nbCommitPhysics( world );
	world->touchedActors.count = 0;

	return (nbDestructibleId){ index + 1, world->worldIndex, world->destructibles.data[index].generation };
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
	while ( world->destructibles.data[index].headActor != NB_NULL_INDEX )
	{
		nbDestroyActor( world, world->destructibles.data[index].headActor );
	}

	destructible = world->destructibles.data + index;
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
