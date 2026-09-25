// SPDX-License-Identifier: MIT

// The impact pipeline:
//  1. query the chunks overlapping the damage sphere
//  2. refine large chunks with a Voronoi fracture concentrated at the impact
//  3. damage the bonds near the impact, broken bonds disconnect chunks
//  4. split actors into rigid islands, islands without an anchor become dynamic bodies
//  5. commit shape changes to Box3D and set the velocities of the new bodies

#include "fracture.h"
#include "world.h"

#include <float.h>

typedef struct nbImpactFrame
{
	int actorIndex;
	b3Vec3 localPoint;
	b3Vec3 localDirection;
} nbImpactFrame;

// A face pair where a refined parent touched a bonded neighbor
typedef struct nbInterface
{
	int neighborIndex;
	int neighborFace;
	b3Plane parentPlane;
	float healthFraction;
} nbInterface;

typedef struct nbQueryContext
{
	nbWorld* world;
	int destructibleIndex;
	int actorIndex;
} nbQueryContext;

static bool nbQueryCallback( b3ShapeId shapeId, void* context )
{
	nbQueryContext* queryContext = context;
	nbWorld* world = queryContext->world;
	int chunkIndex = nbFindChunkFromShape( world, shapeId );
	if ( chunkIndex == NB_NULL_INDEX )
	{
		return true;
	}

	nbChunk* chunk = world->chunks.data + chunkIndex;
	if ( queryContext->destructibleIndex != NB_NULL_INDEX && chunk->destructibleIndex != queryContext->destructibleIndex )
	{
		return true;
	}

	if ( queryContext->actorIndex != NB_NULL_INDEX && chunk->actorIndex != queryContext->actorIndex )
	{
		return true;
	}

	nbArray_Push( world->scratchList, chunkIndex );
	return true;
}

static void nbSortInts( int* values, int count )
{
	for ( int i = 1; i < count; ++i )
	{
		int key = values[i];
		int j = i - 1;
		while ( j >= 0 && values[j] > key )
		{
			values[j + 1] = values[j];
			j -= 1;
		}
		values[j + 1] = key;
	}
}

static const nbImpactFrame* nbFindFrame( const nbImpactFrame* frames, int count, int actorIndex )
{
	for ( int i = 0; i < count; ++i )
	{
		if ( frames[i].actorIndex == actorIndex )
		{
			return frames + i;
		}
	}
	return NULL;
}

// Refine one chunk into Voronoi cells concentrated at the impact. The children replace the chunk in
// its actor, inherit its outer bonds and are glued to each other.
static int nbRefineChunk( nbWorld* world, int chunkIndex, b3Vec3 localPoint, float radius, int innerCount, nbImpactResult* result )
{
	nbChunk* chunk = world->chunks.data + chunkIndex;
	int destructibleIndex = chunk->destructibleIndex;
	int actorIndex = chunk->actorIndex;
	int depth = chunk->depth + 1;
	nbDestructible* destructible = world->destructibles.data + destructibleIndex;
	nbMaterial material = destructible->material;
	const nbShape* parentShape = chunk->shape;
	float parentRadius = parentShape->radius;

	// Center the working polyhedron for precision
	b3Vec3 origin = parentShape->centroid;
	nbPoly* parent = nbArena_AllocArray( &world->arena, nbPoly, 1 );
	nbShape_ToPoly( parentShape, parent );
	nbPoly_Translate( parent, b3Neg( origin ) );

	uint64_t stream = nbHashSeed( destructible->fractureCounter, ( (uint64_t)chunkIndex << 16 ) | chunk->generation );
	destructible->fractureCounter += 1;
	nbRandom rng = nbMakeRandom( destructible->seed, stream );

	float fragmentSize = material.fragmentSize;
	int ringCount = innerCount / 3;
	ringCount = ringCount < 4 ? 4 : ( ringCount > 24 ? 24 : ringCount );

	// Far pieces are cut to about three damage radii
	b3Vec3 extent = b3Sub( parentShape->bounds.upperBound, parentShape->bounds.lowerBound );
	float outerSize = b3MaxFloat( 3.0f * radius, 4.0f * fragmentSize );
	int nx = (int)( extent.x / outerSize ) + 1;
	int ny = (int)( extent.y / outerSize ) + 1;
	int nz = (int)( extent.z / outerSize ) + 1;
	int outerCount = nx * ny * nz - 1;
	outerCount = outerCount > 32 ? 32 : outerCount;

	nbSiteParams params = {
		.center = b3Sub( localPoint, origin ),
		.radius = radius,
		.innerCount = innerCount,
		.ringCount = ringCount,
		.outerCount = outerCount,
		.minSpacing = 0.45f * fragmentSize,
	};

	int capacity = innerCount + ringCount + outerCount;
	b3Vec3* sites = nbArena_AllocArray( &world->arena, b3Vec3, capacity );
	int siteCount = nbGenerateSites( parent, &params, &rng, sites, capacity );
	if ( siteCount < 2 )
	{
		return 0;
	}

	uint64_t ticks = b3GetTicks();
	float tolerance = 1.0e-6f + 2.0e-6f * parentRadius;
	nbFractureOutput output;
	nbComputeVoronoiCells( &world->arena, parent, sites, siteCount, destructible->interiorMaterial, tolerance,
						   material.minFragmentVolume, &output );
	result->fractureTime += b3GetMilliseconds( ticks );

	int validCount = 0;
	for ( int i = 0; i < output.cellCount; ++i )
	{
		validCount += output.cells[i].shape != NULL ? 1 : 0;
	}

	if ( validCount < 2 )
	{
		for ( int i = 0; i < output.cellCount; ++i )
		{
			nbShape_Destroy( output.cells[i].shape );
		}
		return 0;
	}

	// Snapshot the interfaces of the parent with its bonded neighbors: the coplanar face pairs.
	// Children can only touch a neighbor through a face they inherited from the parent face of an
	// interface, so the bonds to the neighbors are found without testing every face pair.
	float contactTolerance = 1.0e-4f + 1.0e-5f * parentRadius;
	int interfaceCapacity = 4 * chunk->bondCount + 1;
	nbInterface* interfaces = nbArena_AllocArray( &world->arena, nbInterface, interfaceCapacity );
	int interfaceCount = 0;
	for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
	{
		const nbBond* bond = world->bonds.data + ( key >> 1 );
		int side = key & 1;
		int neighborIndex = bond->chunk[side ^ 1];
		key = bond->nextKey[side];

		float fullHealth = material.strength * bond->area;
		float healthFraction = fullHealth > 0.0f ? b3ClampFloat( bond->health / fullHealth, 0.0f, 1.0f ) : 0.0f;
		const nbShape* neighborShape = world->chunks.data[neighborIndex].shape;

		for ( int pf = 0; pf < parentShape->faceCount && interfaceCount < interfaceCapacity; ++pf )
		{
			b3Plane parentPlane = parentShape->faces[pf].plane;
			for ( int nf = 0; nf < neighborShape->faceCount; ++nf )
			{
				b3Plane neighborPlane = neighborShape->faces[nf].plane;
				if ( b3Dot( parentPlane.normal, neighborPlane.normal ) > -0.99985f ||
					 b3AbsFloat( parentPlane.offset + neighborPlane.offset ) > contactTolerance )
				{
					continue;
				}

				if ( interfaceCount < interfaceCapacity )
				{
					interfaces[interfaceCount++] = (nbInterface){ neighborIndex, nf, parentPlane, healthFraction };
				}
			}
		}
	}

	// Remove the parent first so the children can take over its place in the actor
	nbDestroyChunk( world, chunkIndex );
	result->fracturedChunkCount += 1;

	int* childIndices = nbArena_AllocArray( &world->arena, int, output.cellCount );
	int childCount = 0;
	for ( int i = 0; i < output.cellCount; ++i )
	{
		childIndices[i] = NB_NULL_INDEX;
		nbShape* shape = output.cells[i].shape;
		if ( shape == NULL )
		{
			continue;
		}

		nbShape_Translate( shape, origin );
		childIndices[i] = nbCreateChunk( world, destructibleIndex, actorIndex, shape, depth );
		childCount += childIndices[i] != NB_NULL_INDEX ? 1 : 0;
	}
	result->createdChunkCount += childCount;

	// Glue the children along their shared Voronoi faces
	float minBondArea = 0.01f * fragmentSize * fragmentSize;
	for ( int i = 0; i < output.cellCount; ++i )
	{
		if ( childIndices[i] == NB_NULL_INDEX )
		{
			continue;
		}

		const nbCell* cell = output.cells + i;
		for ( int k = 0; k < cell->neighborCount; ++k )
		{
			const nbCellNeighbor* neighbor = output.neighbors + cell->firstNeighbor + k;
			int j = neighbor->site;
			if ( j <= i || childIndices[j] == NB_NULL_INDEX || neighbor->area < minBondArea )
			{
				continue;
			}

			b3Vec3 centroid = b3Add( neighbor->centroid, origin );
			nbCreateBond( world, childIndices[i], childIndices[j], neighbor->area, centroid, material.strength * neighbor->area );
		}
	}

	// Glue the children to the former neighbors of the parent where their inherited faces touch
	for ( int i = 0; i < output.cellCount; ++i )
	{
		int childIndex = childIndices[i];
		if ( childIndex == NB_NULL_INDEX )
		{
			continue;
		}

		for ( int k = 0; k < interfaceCount; ++k )
		{
			const nbInterface* face = interfaces + k;
			const nbShape* childShape = world->chunks.data[childIndex].shape;
			for ( int cf = 0; cf < childShape->faceCount; ++cf )
			{
				b3Plane childPlane = childShape->faces[cf].plane;
				if ( b3Dot( childPlane.normal, face->parentPlane.normal ) < 0.99999f ||
					 b3AbsFloat( childPlane.offset - face->parentPlane.offset ) > contactTolerance )
				{
					continue;
				}

				b3Vec3 centroid;
				float area =
					nbShape_FaceOverlap( childShape, cf, world->chunks.data[face->neighborIndex].shape, face->neighborFace, &centroid );
				float health = material.strength * area * face->healthFraction;
				if ( area > minBondArea && health > 0.0f )
				{
					nbCreateBond( world, childIndex, face->neighborIndex, area, centroid, health );
				}
				break;
			}
		}
	}

	return childCount;
}

static void nbApplyVelocities( nbWorld* world, const nbImpactDef* def, nbRandom* rng, const int* hitActors, int hitActorCount )
{
	b3Vec3 direction = b3Normalize( def->direction );
	bool isDirected = b3LengthSquared( direction ) > 0.5f;
	float reach = 1.5f * def->radius;

	for ( int i = 0; i < world->touchedActors.count; ++i )
	{
		int actorIndex = world->touchedActors.data[i];
		nbActor* actor = world->actors.data + actorIndex;
		if ( actor->isFree || actor->isStatic || actor->isNew == false )
		{
			continue;
		}
		actor->isNew = false;

		// Inherit the velocity field of the source body
		b3Pos center = b3Body_GetWorldCenter( actor->bodyId );
		b3Vec3 offset = b3SubPos( center, actor->sourceCenter );
		b3Vec3 linearVelocity = b3Add( actor->sourceLinearVelocity, b3Cross( actor->sourceAngularVelocity, offset ) );
		b3Vec3 angularVelocity = actor->sourceAngularVelocity;

		if ( def->ejectSpeed > 0.0f )
		{
			b3Vec3 delta = b3SubPos( center, def->point );
			float distance;
			b3Vec3 radial = b3GetLengthAndNormalize( &distance, delta );
			if ( distance < reach )
			{
				float falloff = 1.0f - distance / reach;
				if ( distance < 1.0e-4f )
				{
					radial = isDirected ? direction : b3Vec3_axisY;
				}

				b3Vec3 jitter = nbRandomUnitVector( rng );
				b3Vec3 eject;
				if ( isDirected )
				{
					eject = b3Add( b3Add( b3MulSV( 0.8f, direction ), b3MulSV( 0.6f, radial ) ), b3MulSV( 0.35f, jitter ) );
				}
				else
				{
					eject = b3Add( radial, b3MulSV( 0.3f, jitter ) );
				}

				float speed = def->ejectSpeed * falloff * nbRandomRange( rng, 0.6f, 1.0f );
				linearVelocity = b3MulAdd( linearVelocity, speed, b3Normalize( eject ) );
				angularVelocity = b3MulAdd( angularVelocity, falloff * nbRandomRange( rng, 3.0f, 18.0f ), nbRandomUnitVector( rng ) );
			}
		}

		b3Body_SetLinearVelocity( actor->bodyId, linearVelocity );
		b3Body_SetAngularVelocity( actor->bodyId, angularVelocity );
	}

	// Loose debris inside the blast gets pushed as well. Heavy parts barely move.
	if ( def->ejectSpeed <= 0.0f )
	{
		return;
	}

	for ( int i = 0; i < hitActorCount; ++i )
	{
		nbActor* actor = world->actors.data + hitActors[i];
		if ( actor->isFree || actor->isStatic || actor->chunkCount == 0 )
		{
			continue;
		}

		const nbDestructible* destructible = world->destructibles.data + actor->destructibleIndex;
		float fragmentVolume = destructible->material.fragmentSize * destructible->material.fragmentSize *
							   destructible->material.fragmentSize;

		b3Pos center = b3Body_GetWorldCenter( actor->bodyId );
		b3Vec3 delta = b3SubPos( center, def->point );
		float distance;
		b3Vec3 radial = b3GetLengthAndNormalize( &distance, delta );
		if ( distance >= reach )
		{
			continue;
		}

		float falloff = 1.0f - distance / reach;
		float scale = b3MinFloat( 1.0f, 4.0f * fragmentVolume / b3MaxFloat( actor->volume, 1.0e-9f ) );
		b3Vec3 push = isDirected ? b3Normalize( b3Add( direction, b3MulSV( 0.5f, radial ) ) ) : radial;
		b3Vec3 velocity = b3Body_GetLinearVelocity( actor->bodyId );
		velocity = b3MulAdd( velocity, 0.5f * def->ejectSpeed * falloff * scale, push );
		b3Body_SetLinearVelocity( actor->bodyId, velocity );
		b3Body_SetAwake( actor->bodyId, true );
	}
}

nbImpactResult nbApplyImpact( nbWorld* world, const nbImpactDef* def, int actorFilter )
{
	nbImpactResult result = { 0 };
	if ( def->radius <= 0.0f )
	{
		return result;
	}

	uint64_t ticks = b3GetTicks();
	nbBeginOperation( world );
	world->scratchList.count = 0;

	int destructibleFilter = NB_NULL_INDEX;
	if ( NB_IS_NON_NULL( def->destructibleId ) )
	{
		if ( nbGetDestructibleFromId( def->destructibleId, NULL ) == NULL )
		{
			return result;
		}
		destructibleFilter = def->destructibleId.index1 - 1;
	}

	// 1. Query
	float radius = def->radius;
	b3Pos point = def->point;
	b3AABB box = {
		{ (float)point.x - radius, (float)point.y - radius, (float)point.z - radius },
		{ (float)point.x + radius, (float)point.y + radius, (float)point.z + radius },
	};

	nbQueryContext queryContext = { world, destructibleFilter, actorFilter };
	b3World_OverlapAABB( world->physicsWorld, box, b3DefaultQueryFilter(), nbQueryCallback, &queryContext );

	int queryCount = world->scratchList.count;
	if ( queryCount == 0 )
	{
		return result;
	}

	// Box3D reports in tree order. Sort for a deterministic result.
	nbSortInts( world->scratchList.data, queryCount );

	int seedDestructible = world->chunks.data[world->scratchList.data[0]].destructibleIndex;
	int* candidates = nbArena_AllocArray( &world->arena, int, queryCount );
	float* overlaps = nbArena_AllocArray( &world->arena, float, queryCount );
	nbImpactFrame* frames = nbArena_AllocArray( &world->arena, nbImpactFrame, queryCount );
	int* hitActors = nbArena_AllocArray( &world->arena, int, queryCount );
	int candidateCount = 0;
	int frameCount = 0;

	for ( int i = 0; i < queryCount; ++i )
	{
		int chunkIndex = world->scratchList.data[i];
		if ( i > 0 && chunkIndex == world->scratchList.data[i - 1] )
		{
			continue;
		}

		nbChunk* chunk = world->chunks.data + chunkIndex;
		const nbImpactFrame* frame = nbFindFrame( frames, frameCount, chunk->actorIndex );
		if ( frame == NULL )
		{
			nbActor* actor = world->actors.data + chunk->actorIndex;
			b3WorldTransform transform = nbActor_GetTransform( world, actor );
			nbImpactFrame* newFrame = frames + frameCount;
			newFrame->actorIndex = chunk->actorIndex;
			newFrame->localPoint = b3InvTransformWorldPoint( transform, point );
			newFrame->localDirection = b3InvRotateVector( transform.q, def->direction );
			hitActors[frameCount] = chunk->actorIndex;
			frameCount += 1;
			frame = newFrame;
		}

		float distance = nbShape_Distance( chunk->shape, frame->localPoint );
		if ( distance > radius )
		{
			continue;
		}

		candidates[candidateCount++] = chunkIndex;
	}

	if ( candidateCount == 0 )
	{
		return result;
	}

	world->stats.impactCount += 1;

	// 2. Refine chunks that are much larger than the fragments this impact creates
	int refineCount = 0;
	float totalOverlap = 0.0f;
	for ( int i = 0; i < candidateCount; ++i )
	{
		nbChunk* chunk = world->chunks.data + candidates[i];
		const nbDestructible* destructible = world->destructibles.data + chunk->destructibleIndex;
		const nbMaterial* material = &destructible->material;
		float fragmentSize = material->fragmentSize;
		float fragmentVolume = fragmentSize * fragmentSize * fragmentSize;

		overlaps[i] = 0.0f;
		bool canRefine = chunk->depth < material->maxDepth && chunk->shape->volume > 3.0f * fragmentVolume &&
						 chunk->shape->radius > 1.2f * fragmentSize;
		if ( canRefine == false )
		{
			continue;
		}

		const nbImpactFrame* frame = nbFindFrame( frames, frameCount, chunk->actorIndex );
		nbPoly* poly = nbArena_AllocArray( &world->arena, nbPoly, 1 );
		nbShape_ToPoly( chunk->shape, poly );
		nbRandom sampler = nbMakeRandom( destructible->seed, nbHashSeed( candidates[i], chunk->generation ) );
		overlaps[i] = nbEstimateSphereOverlap( poly, frame->localPoint, radius, &sampler, 64 );
		totalOverlap += overlaps[i];
		refineCount += overlaps[i] > 0.0f ? 1 : 0;
	}

	int firstChild = world->touchedChunks.count;
	for ( int i = 0; i < candidateCount && refineCount > 0; ++i )
	{
		if ( overlaps[i] <= 0.0f )
		{
			continue;
		}

		int chunkIndex = candidates[i];
		nbChunk* chunk = world->chunks.data + chunkIndex;
		const nbMaterial* material = &world->destructibles.data[chunk->destructibleIndex].material;
		float fragmentSize = material->fragmentSize;
		float fragmentVolume = fragmentSize * fragmentSize * fragmentSize;

		int innerCount;
		if ( def->fragmentCount > 0 )
		{
			innerCount = (int)( (float)def->fragmentCount * overlaps[i] / totalOverlap + 0.5f );
		}
		else
		{
			innerCount = (int)( 0.5f * overlaps[i] / fragmentVolume + 0.5f );
		}
		innerCount = innerCount < 3 ? 3 : ( innerCount > 192 ? 192 : innerCount );

		const nbImpactFrame* frame = nbFindFrame( frames, frameCount, chunk->actorIndex );
		int created = nbRefineChunk( world, chunkIndex, frame->localPoint, radius, innerCount, &result );
		if ( created > 0 )
		{
			candidates[i] = NB_NULL_INDEX;
		}
	}

	// 3. Damage bonds around the impact. Every bond is visited once.
	world->bondStamp += 1;
	uint32_t stamp = world->bondStamp;
	int touchedEnd = world->touchedChunks.count;
	int damageCount = candidateCount + ( touchedEnd - firstChild );
	int* damageChunks = nbArena_AllocArray( &world->arena, int, damageCount );
	int damageChunkCount = 0;
	for ( int i = 0; i < candidateCount; ++i )
	{
		if ( candidates[i] != NB_NULL_INDEX )
		{
			damageChunks[damageChunkCount++] = candidates[i];
		}
	}
	for ( int i = firstChild; i < touchedEnd; ++i )
	{
		int chunkIndex = world->touchedChunks.data[i];
		if ( world->chunks.data[chunkIndex].shape != NULL )
		{
			damageChunks[damageChunkCount++] = chunkIndex;
		}
	}

	for ( int i = 0; i < damageChunkCount; ++i )
	{
		int chunkIndex = damageChunks[i];
		nbChunk* chunk = world->chunks.data + chunkIndex;
		if ( chunk->shape == NULL )
		{
			continue;
		}

		const nbImpactFrame* frame = nbFindFrame( frames, frameCount, chunk->actorIndex );
		if ( frame == NULL )
		{
			continue;
		}

		for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
		{
			int bondIndex = key >> 1;
			nbBond* bond = world->bonds.data + bondIndex;
			int side = key & 1;
			key = bond->nextKey[side];

			if ( bond->stamp == stamp )
			{
				continue;
			}
			bond->stamp = stamp;

			float distance = b3Distance( bond->centroid, frame->localPoint );
			if ( distance >= radius )
			{
				continue;
			}

			bond->health -= def->damage * ( 1.0f - distance / radius );
			if ( bond->health <= 0.0f )
			{
				nbDestroyBond( world, bondIndex );
				result.brokenBondCount += 1;
			}
		}
	}

	// 4. Islands
	nbSplitActors( world, &result );

	// 5. Physics
	nbCommitPhysics( world );

	nbRandom rng = nbMakeRandom( world->destructibles.data[seedDestructible].seed, nbHashSeed( world->stats.impactCount, 0xe7ec7 ) );
	nbApplyVelocities( world, def, &rng, hitActors, frameCount );

	world->touchedActors.count = 0;

	result.totalTime = b3GetMilliseconds( ticks );
	world->stats.lastImpactTime = result.totalTime;
	world->stats.lastFractureTime = result.fractureTime;
	world->stats.fractureCount += result.fracturedChunkCount;
	return result;
}

nbImpactResult nbWorld_ApplyImpact( nbWorldId worldId, const nbImpactDef* def )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return (nbImpactResult){ 0 };
	}
	return nbApplyImpact( world, def, NB_NULL_INDEX );
}

bool nbWorld_CastImpact( nbWorldId worldId, b3Pos origin, b3Vec3 translation, const nbImpactDef* def, nbImpactResult* result )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return false;
	}

	b3RayResult ray = b3World_CastRayClosest( world->physicsWorld, origin, translation, b3DefaultQueryFilter() );
	if ( ray.hit == false )
	{
		return false;
	}

	int chunkIndex = nbFindChunkFromShape( world, ray.shapeId );
	if ( chunkIndex == NB_NULL_INDEX )
	{
		return false;
	}

	if ( NB_IS_NON_NULL( def->destructibleId ) && world->chunks.data[chunkIndex].destructibleIndex != def->destructibleId.index1 - 1 )
	{
		return false;
	}

	nbImpactDef impact = *def;
	impact.point = ray.point;
	impact.direction = b3Normalize( translation );

	nbImpactResult impactResult = nbWorld_ApplyImpact( worldId, &impact );
	if ( result != NULL )
	{
		*result = impactResult;
	}
	return true;
}
