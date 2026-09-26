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
	material.maxSpan = 0.0f;
	material.tensileStrength = 2.0e6f;
	material.compressiveStrength = 3.0e7f;
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
	def.jointTensileStrength = -1.0f;
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

// Jittered grid sites for an even pre-fracture
static int nbGenerateCellSites( const nbPoly* poly, float cellSize, nbRandom* rng, b3Vec3* sites, int capacity )
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

				if ( nbPoly_ContainsPoint( poly, p, 0.0f ) )
				{
					sites[count++] = p;
				}
			}
		}
	}
	return count;
}

// Draw the pre-fracture sites of a piece. Returns false if the piece stays a single chunk.
static bool nbPreparePiece( nbWorld* world, const nbMaterial* material, nbPoly* poly, float cellSize, uint8_t interiorMaterial,
							nbRandom* rng, nbFractureJob* job )
{
	if ( cellSize <= 0.0f )
	{
		return false;
	}

	// Work in coordinates centered on the piece for precision
	float volume;
	b3Vec3 origin;
	nbPoly_ComputeMass( poly, &volume, &origin );
	nbPoly_Translate( poly, b3Neg( origin ) );

	int capacity = 2048;
	b3Vec3* sites = nbArena_AllocArray( &world->arena, b3Vec3, capacity );
	int siteCount = nbGenerateCellSites( poly, cellSize, rng, sites, capacity );
	if ( siteCount < 2 )
	{
		nbPoly_Translate( poly, origin );
		return false;
	}

	float radius = sqrtf( nbPoly_MaxDistanceSquared( poly, b3Vec3_zero ) );
	*job = (nbFractureJob){
		.parent = poly,
		.sites = sites,
		.siteCount = siteCount,
		.origin = origin,
		.interiorMaterial = interiorMaterial,
		.tolerance = 1.0e-6f + 2.0e-6f * radius,
		.minVolume = material->minFragmentVolume,
		.buildHulls = true,
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

// Create the chunks of a pre-fractured piece and glue them exactly like a single piece
static void nbFinishPiece( nbWorld* world, int destructibleIndex, int actorIndex, const nbFractureJob* job, int materialIndex )
{
	const nbMaterial* material = world->destructibles.data[destructibleIndex].materials + materialIndex;

	int* chunkIndices = nbArena_AllocArray( &world->arena, int, job->siteCount );
	for ( int i = 0; i < job->siteCount; ++i )
	{
		chunkIndices[i] = NB_NULL_INDEX;
		const nbCell* cell = job->cells + i;
		if ( cell->shape == NULL )
		{
			continue;
		}

		chunkIndices[i] =
			nbCreateChunkWithHull( world, destructibleIndex, actorIndex, cell->shape, cell->hull, 0, job->interiorMaterial, materialIndex );
	}

	float minBondArea = 0.01f * material->fragmentSize * material->fragmentSize;
	float tensileStrength = nbGetTensileStrength( material, material );
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
			nbCreateBond( world, chunkIndices[i], chunkIndices[j], &geometry, material->strength * geometry.area, tensileStrength );
		}
	}
}

static bool nbMaterialEquals( const nbMaterial* a, const nbMaterial* b )
{
	return a->density == b->density && a->friction == b->friction && a->restitution == b->restitution && a->strength == b->strength &&
		   a->fragmentSize == b->fragmentSize && a->minFragmentVolume == b->minFragmentVolume && a->maxDepth == b->maxDepth &&
		   a->maxSpan == b->maxSpan && a->tensileStrength == b->tensileStrength && a->compressiveStrength == b->compressiveStrength &&
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
		world->destructibles.data[index].hasJoints = world->destructibles.data[index].hasJoints || pieces[i].jointTensileStrength >= 0.0f;
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
		if ( valid && nbPreparePiece( world, pieceMaterial, piecePoly, def->cellSize, piece->interiorMaterial, &rng, jobs + jobCount ) )
		{
			pieceJobs[i] = jobCount;
			jobCount += 1;
		}
	}

	nbRunFractureJobs( world, jobs, jobCount );

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
		}
		else
		{
			nbFinishPiece( world, index, actorIndex, jobs + pieceJobs[i], pieceMaterials[i] );
		}

		// Remember which piece each chunk came from, so bonds between pieces can be found
		for ( int k = start; k < world->touchedChunks.count; ++k )
		{
			world->chunks.data[world->touchedChunks.data[k]].scratch = i;
		}
	}

	// Glue touching faces of different pieces. A bond between two materials is as strong as the weaker one, a
	// joint between pieces at most as strong as the joint strength of either piece.
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
			float fragmentSize = b3MinFloat( materialA->fragmentSize, materialB->fragmentSize );
			float minBondArea = 0.01f * fragmentSize * fragmentSize;

			nbBondGeometry geometry;
			float area = nbShape_ContactArea( world->chunks.data[chunkA].shape, world->chunks.data[chunkB].shape, 1.0e-3f, &geometry );
			if ( area > minBondArea )
			{
				float tensileStrength = nbGetTensileStrength( materialA, materialB );
				for ( int k = 0; k < 2; ++k )
				{
					float joint = pieces[k == 0 ? pieceA : pieceB].jointTensileStrength;
					tensileStrength = joint >= 0.0f ? b3MinFloat( tensileStrength, joint ) : tensileStrength;
				}
				nbCreateBond( world, chunkA, chunkB, &geometry, b3MinFloat( materialA->strength, materialB->strength ) * area,
							  tensileStrength );
			}
		}
	}

	for ( int k = firstNewChunk; k < chunkEnd; ++k )
	{
		world->chunks.data[world->touchedChunks.data[k]].scratch = NB_NULL_INDEX;
	}

	nbCommitPhysics( world );
	world->touchedActors.count = 0;

	// The first update checks whether the structure carries its own weight
	world->destructibles.data[index].structureDirty = def->isStatic;

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
