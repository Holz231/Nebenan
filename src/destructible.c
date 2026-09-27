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

// The cells of a pre-fracture carry together as one block in the load check, see nbCheckSupport, and a block that
// collapses falls apart into groups about this many cells across. The groups are the Voronoi cells of a coarser set of
// sites, so they end along the faces of the cells.
#define NB_GROUP_SIZE 2.5f

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

// Draw the pre-fracture sites of a piece, and the block of every site. Returns false if the piece stays a single chunk.
static bool nbPreparePiece( nbWorld* world, const nbMaterial* material, const nbPieceDef* piece, nbPoly* poly, float cellSize,
							nbRandom* rng, nbFractureJob* job, int** blocksOut )
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
	int capacity = 2048;
	b3Vec3* sites = nbArena_AllocArray( &world->arena, b3Vec3, capacity );
	int siteCount = cellSize > 0.0f ? nbGenerateCellSites( poly, cellSize, cutouts, cutoutCount, rng, sites, capacity ) : 0;
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

	// Every cell joins the group of the nearest coarse site
	int* blocks = nbArena_AllocArray( &world->arena, int, siteCount );
	b3Vec3* blockSites = nbArena_AllocArray( &world->arena, b3Vec3, capacity );
	int blockCount =
		siteCount > 1 ? nbGenerateCellSites( poly, NB_GROUP_SIZE * cellSize, NULL, 0, rng, blockSites, capacity ) : 0;
	for ( int i = 0; i < siteCount; ++i )
	{
		blocks[i] = 0;
		float best = FLT_MAX;
		for ( int k = 0; k < blockCount; ++k )
		{
			float distance = b3DistanceSquared( sites[i], blockSites[k] );
			if ( distance < best )
			{
				best = distance;
				blocks[i] = k;
			}
		}
	}
	*blocksOut = blocks;

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

// Create the chunks of a pre-fractured piece and glue them exactly like a single piece, with the block of every new chunk
// in the order they are created. The parts of cells that openings cut are glued by nbCreateDestructible.
static void nbFinishPiece( nbWorld* world, int destructibleIndex, int actorIndex, const nbFractureJob* job, const int* blocks,
						   int materialIndex, nbIntArray* chunkBlocks )
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
				nbArray_Push( *chunkBlocks, blocks[i] );
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
			nbArray_Push( *chunkBlocks, blocks[i] );
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
			world->bonds.data[bondIndex].fault = blocks[i] != blocks[j];
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
	int* pieceJobs = nbArena_AllocArray( &world->arena, int, pieceCount );
	nbFractureJob* jobs = nbArena_AllocArray( &world->arena, nbFractureJob, pieceCount );
	int** jobBlocks = nbArena_AllocArray( &world->arena, int*, pieceCount );
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

		pieceJobs[i] = valid ? nb_pieceSingle : nb_pieceInvalid;
		const nbMaterial* pieceMaterial = world->destructibles.data[index].materials + pieceMaterials[i];
		float cellSize = piece->cellSize != 0.0f ? piece->cellSize : def->cellSize;
		if ( valid &&
			 nbPreparePiece( world, pieceMaterial, piece, piecePoly, cellSize, &rng, jobs + jobCount, jobBlocks + jobCount ) )
		{
			pieceJobs[i] = jobCount;
			jobCount += 1;
		}
	}

	nbRunFractureJobs( world, jobs, jobCount );

	// The piece and block of every new chunk, in the order of the touched chunks
	nbIntArray chunkPieces = { 0 };
	nbIntArray chunkBlocks = { 0 };
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
				nbArray_Push( chunkBlocks, 0 );
			}
		}
		else
		{
			nbFinishPiece( world, index, actorIndex, jobs + pieceJobs[i], jobBlocks[pieceJobs[i]], pieceMaterials[i],
						   &chunkBlocks );
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

	// Glue touching faces of different pieces. A bond between two materials is as strong as the weaker one.
	int chunkEnd = world->touchedChunks.count;
	for ( int a = firstNewChunk; a < chunkEnd; ++a )
	{
		int chunkA = world->touchedChunks.data[a];
		int pieceA = world->chunks.data[chunkA].scratch;
		for ( int b = a + 1; b < chunkEnd; ++b )
		{
			int chunkB = world->touchedChunks.data[b];
			int pieceB = world->chunks.data[chunkB].scratch;
			if ( pieceA == pieceB )
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

				// Two cells of a piece with openings, in the same group or not
				int cellA = a - firstNewChunk, cellB = b - firstNewChunk;
				bool samePiece = chunkPieces.data[cellA] == chunkPieces.data[cellB];
				world->bonds.data[bondIndex].cohesive = samePiece;
				world->bonds.data[bondIndex].fault = samePiece && chunkBlocks.data[cellA] != chunkBlocks.data[cellB];
			}
		}
	}
	nbArray_Free( chunkPieces );
	nbArray_Free( chunkBlocks );

	for ( int k = firstNewChunk; k < chunkEnd; ++k )
	{
		world->chunks.data[world->touchedChunks.data[k]].scratch = NB_NULL_INDEX;
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
