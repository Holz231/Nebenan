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
	def.interiorMaterial = 1;
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

				// Jitter inside the grid cell but keep away from the cell borders to avoid slivers
				b3Vec3 p = {
					bounds.lowerBound.x + step.x * ( (float)i + nbRandomRange( rng, 0.15f, 0.85f ) ),
					bounds.lowerBound.y + step.y * ( (float)j + nbRandomRange( rng, 0.15f, 0.85f ) ),
					bounds.lowerBound.z + step.z * ( (float)k + nbRandomRange( rng, 0.15f, 0.85f ) ),
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

// Split a piece into cells and create chunks with internal bonds. Returns the number of chunks.
static int nbCreatePieceChunks( nbWorld* world, int destructibleIndex, int actorIndex, nbPoly* poly, float cellSize,
								nbRandom* rng )
{
	nbDestructible* destructible = world->destructibles.data + destructibleIndex;
	const nbMaterial* material = &destructible->material;

	if ( cellSize <= 0.0f )
	{
		nbShape* shape = nbShape_Create( poly );
		if ( shape == NULL )
		{
			return 0;
		}
		return nbCreateChunk( world, destructibleIndex, actorIndex, shape, 0 ) != NB_NULL_INDEX ? 1 : 0;
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
		nbShape* shape = nbShape_Create( poly );
		if ( shape == NULL )
		{
			return 0;
		}
		return nbCreateChunk( world, destructibleIndex, actorIndex, shape, 0 ) != NB_NULL_INDEX ? 1 : 0;
	}

	float radius = sqrtf( nbPoly_MaxDistanceSquared( poly, b3Vec3_zero ) );
	float tolerance = 1.0e-6f + 2.0e-6f * radius;

	nbFractureOutput output;
	nbComputeVoronoiCells( &world->arena, poly, sites, siteCount, destructible->interiorMaterial, tolerance,
						   material->minFragmentVolume, &output );

	int* chunkIndices = nbArena_AllocArray( &world->arena, int, siteCount );
	int chunkCount = 0;
	for ( int i = 0; i < output.cellCount; ++i )
	{
		chunkIndices[i] = NB_NULL_INDEX;
		nbShape* shape = output.cells[i].shape;
		if ( shape == NULL )
		{
			continue;
		}

		nbShape_Translate( shape, origin );
		chunkIndices[i] = nbCreateChunk( world, destructibleIndex, actorIndex, shape, 0 );
		chunkCount += chunkIndices[i] != NB_NULL_INDEX ? 1 : 0;
	}

	// Pre-fractured cells are separate chunks but glued exactly like a single piece
	float minBondArea = 0.01f * material->fragmentSize * material->fragmentSize;
	for ( int i = 0; i < output.cellCount; ++i )
	{
		if ( chunkIndices[i] == NB_NULL_INDEX )
		{
			continue;
		}

		const nbCell* cell = output.cells + i;
		for ( int k = 0; k < cell->neighborCount; ++k )
		{
			const nbCellNeighbor* neighbor = output.neighbors + cell->firstNeighbor + k;
			int j = neighbor->site;
			if ( j <= i || chunkIndices[j] == NB_NULL_INDEX || neighbor->area < minBondArea )
			{
				continue;
			}

			b3Vec3 centroid = b3Add( neighbor->centroid, origin );
			nbCreateBond( world, chunkIndices[i], chunkIndices[j], neighbor->area, centroid, material->strength * neighbor->area );
		}
	}

	return chunkCount;
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
	destructible->material = def->material;
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
	destructible->interiorMaterial = def->interiorMaterial;
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

	nbRandom rng = nbMakeRandom( def->seed, 0x5eed );
	int firstNewChunk = world->touchedChunks.count;

	for ( int i = 0; i < pieceCount; ++i )
	{
		const nbPieceDef* piece = pieces + i;
		bool valid = true;
		if ( piece->points != NULL )
		{
			b3HullData* hull = b3CreateHull( piece->points, piece->pointCount, B3_MAX_HULL_VERTICES );
			valid = hull != NULL && nbPoly_MakeFromHull( poly, hull, piece->transform, piece->surfaceMaterial );
			if ( hull != NULL )
			{
				b3DestroyHull( hull );
			}
		}
		else
		{
			nbPoly_MakeBox( poly, piece->halfExtents, piece->transform, piece->surfaceMaterial );
		}

		if ( valid )
		{
			int start = world->touchedChunks.count;
			nbCreatePieceChunks( world, index, actorIndex, poly, def->cellSize, &rng );

			// Remember which piece each chunk came from, so bonds between pieces can be found
			for ( int k = start; k < world->touchedChunks.count; ++k )
			{
				world->chunks.data[world->touchedChunks.data[k]].scratch = i;
			}
		}
	}

	// Glue touching faces of different pieces
	int chunkEnd = world->touchedChunks.count;
	const nbMaterial* material = &world->destructibles.data[index].material;
	float minBondArea = 0.01f * material->fragmentSize * material->fragmentSize;
	for ( int a = firstNewChunk; a < chunkEnd; ++a )
	{
		int chunkA = world->touchedChunks.data[a];
		for ( int b = a + 1; b < chunkEnd; ++b )
		{
			int chunkB = world->touchedChunks.data[b];
			if ( world->chunks.data[chunkA].scratch == world->chunks.data[chunkB].scratch )
			{
				continue;
			}

			b3Vec3 centroid, normal;
			float area =
				nbShape_ContactArea( world->chunks.data[chunkA].shape, world->chunks.data[chunkB].shape, 1.0e-3f, &centroid, &normal );
			if ( area > minBondArea )
			{
				nbCreateBond( world, chunkA, chunkB, area, centroid, material->strength * area );
			}
		}
	}

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
